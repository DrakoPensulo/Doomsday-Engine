/**
 * @file protocol.c
 * Implementation of the network protocol. @ingroup network
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

/**
 * @page sysNetwork Low-Level Networking
 *
 * On server-side connected clients can be either in "unjoined" mode or
 * "joined" mode. The former is for querying information about the server's
 * status, while the latter one is for clients participating in the on-going
 * game.
 *
 * Unjoined TCP sockets are periodically polled for activity
 * (N_ListenUnjoinedNodes()). Joined TCP sockets are handled in a separate
 * receiver thread (N_JoinedListenerThread()).
 *
 * @section netProtocol Network Protocol
 *
 * In joined mode, the network protocol works as follows. All messages are sent
 * over a TCP socket. Every message consists of a header and the message payload.
 * The content of these depends on the compressed message size.
 *
 * @par 1&ndash;127 bytes
 * Very small messages, such as the position updates that a client streams
 * to the server, are encoded with Huffman codes (see huffman.h). If
 * the Huffman coded payload happens to exceed 127 bytes, the message is
 * switched to the medium format (see below). Message structure:
 * - 1 byte: payload size
 * - @em n bytes: payload contents (Huffman)
 *
 * @par 128&ndash;4095 bytes
 * Medium-sized messages are compressed either using a fast zlib deflate level,
 * or Huffman codes if it yields better compression.
 * If the deflated message size exceeds 4095 bytes, the message is switched to
 * the large format (see below). Message structure:
 * - 1 byte: 0x80 | (payload size & 0x7f)
 * - 1 byte: (payload size >> 7) | (0x40 for deflated, otherwise Huffman)
 * - @em n bytes: payload contents (as produced by ZipFile_CompressAtLevel()).
 *
 * @par >= 4096 bytes (up to 4MB)
 * Large messages are compressed using the best zlib deflate level.
 * Message structure:
 * - 1 byte: 0x80 | (payload size & 0x7f)
 * - 1 byte: 0x80 | (payload size >> 7) & 0x7f
 * - 1 byte: payload size >> 14
 * - @em n bytes: payload contents (as produced by ZipFile_CompressAtLevel()).
 *
 * Messages larger than or equal to 2^22 bytes (about 4MB) must be broken into
 * smaller pieces before sending.
 *
 * @see Protocol_Send()
 * @see Protocol_Receive()
 */

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_misc.h"
#include "protocol.h"

#include "sys_network.h"
#include "huffman.h"
#include "zipfile.h"

#define MAX_SIZE_SMALL          127 // bytes
#define MAX_SIZE_MEDIUM         4095 // bytes
#define MAX_SIZE_LARGE          PROTOCOL_MAX_DATAGRAM_SIZE

/// Threshold for input data size: messages smaller than this are first compressed
/// with Doomsday's Huffman codes. If the result is smaller than the deflated data,
/// the Huffman coded payload is used (unless it doesn't fit in a medium-sized packet).
#define MAX_HUFFMAN_INPUT_SIZE  4096 // bytes

#define DEFAULT_TRANSMISSION_SIZE   4096

#define TRMF_CONTINUE           0x80
#define TRMF_DEFLATED           0x40
#define TRMF_SIZE_MASK          0x7f
#define TRMF_SIZE_MASK_MEDIUM   0x3f
#define TRMF_SIZE_SHIFT         7

static byte* transmissionBuffer;
static size_t transmissionBufferSize;

void Protocol_Init(void)
{
    // Allocate the transmission buffer.
    transmissionBufferSize = DEFAULT_TRANSMISSION_SIZE;
    transmissionBuffer = M_Malloc(transmissionBufferSize);
}

void Protocol_Shutdown(void)
{
    M_Free(transmissionBuffer);
    transmissionBuffer = NULL;
    transmissionBufferSize = 0;
}

#if 0
static boolean getBytesBlocking(TCPsocket sock, byte* buffer, size_t size)
{
    int bytes = 0;
    while(bytes < (int)size)
    {
        int result = SDLNet_TCP_Recv(sock, buffer + bytes, size - bytes);
        if(result <= 0)
            return false; // Socket closed or an error occurred.

        bytes += result;
        assert(bytes <= (int)size);
    }
    return true;
}

static boolean getNextByte(TCPsocket sock, byte* b)
{
    return getBytesBlocking(sock, b, 1);
}
#endif

boolean Protocol_Receive(nodeid_t from)
{
    int     size = 0;
    int     sock = netNodes[from].sock;
    byte*   packet = 0;

    packet = LegacyNetwork_Receive(sock, &size);
    if(!packet)
    {
        // Failed to receive anything.
        return false;
    }

    // Post the received message.
    {
        netmessage_t *msg = M_Calloc(sizeof(netmessage_t));

        msg->sender = from;
        msg->data = packet;
        msg->size = size;
        msg->handle = packet; // needs to be freed

/*#ifdef _DEBUG
        VERBOSE2(Con_Message("N_ReceiveReliably: Posting message, from=%i, size=%i\n", from, size));
#endif*/

        // The message queue will handle the message from now on.
        N_PostMessage(msg);
    }
    return true;

#if 0
    TCPsocket sock = (TCPsocket) N_GetNodeSocket(from);
    byte* packet = 0;
    size_t size = 0;
    boolean needInflate = false;
    byte b;

    // Read the header.
    if(!getNextByte(sock, &b)) return false;

    size = b & TRMF_SIZE_MASK;

    if(b & TRMF_CONTINUE)
    {
        if(!getNextByte(sock, &b)) return false;

        if(b & TRMF_CONTINUE)
        {
            // Large header.
            needInflate = true;
            size |= ((b & TRMF_SIZE_MASK) << TRMF_SIZE_SHIFT);

            if(!getNextByte(sock, &b)) return false;

            size |= (b << (2*TRMF_SIZE_SHIFT));
        }
        else
        {
            // Medium header.
            if(b & TRMF_DEFLATED) needInflate = true;
            size |= ((b & TRMF_SIZE_MASK_MEDIUM) << TRMF_SIZE_SHIFT);
        }
    }

    // Allocate memory for the packet. This will be freed once the message
    // has been handled.
    packet = (byte*) M_Malloc(size);

    if(!getBytesBlocking(sock, packet, size))
    {
        // An error with the socket (closed?).
        M_Free(packet);
        return false;
    }

    // Uncompress the payload.
    {
        size_t uncompSize = 0;
        byte* uncompData = 0;
        if(needInflate)
        {
            uncompData = ZipFile_Uncompress(packet, size, &uncompSize);
        }
        else
        {
            uncompData = Huffman_Decode(packet, size, &uncompSize);
        }
        if(!uncompData)
        {
            M_Free(packet);
            return false;
        }
        // Replace with the uncompressed version.
        M_Free(packet);
        packet = uncompData;
        size = uncompSize;
    }

    // Post the received message.
    {
        netmessage_t *msg = M_Calloc(sizeof(netmessage_t));
        msg->sender = from;
        msg->data = packet;
        msg->size = size;
        msg->handle = packet;

        // The message queue will handle the message from now on.
        N_PostMessage(msg);
    }
    return true;
#endif
}

void Protocol_FreeBuffer(void *handle)
{
    if(handle)
    {
        LegacyNetwork_FreeBuffer(handle);
    }
}

static void checkTransmissionBufferSize(size_t atLeastBytes)
{
    while(transmissionBufferSize < atLeastBytes)
    {
        transmissionBufferSize *= 2;
        transmissionBuffer = M_Realloc(transmissionBuffer, transmissionBufferSize);
    }
}

/**
 * Copies the message payload @a data to the transmission buffer.
 *
 * @param data  Data payload being send.
 * @param size  Size of the data payload.
 * @param needInflate  @c true, if the payload is deflated.
 */
static size_t prepareTransmission(void* data, size_t size, boolean needInflate)
{
    Writer* msg = 0;
    size_t msgSize = 0;

    // The header is at most 3 bytes.
    checkTransmissionBufferSize(size + 3);

    // Compose the header and payload into the transmission buffer.
    msg = Writer_NewWithBuffer(transmissionBuffer, transmissionBufferSize);

    if(size <= MAX_SIZE_SMALL && !needInflate)
    {
        Writer_WriteByte(msg, size);
    }
    else if(size <= MAX_SIZE_MEDIUM)
    {
        Writer_WriteByte(msg, TRMF_CONTINUE | (size & TRMF_SIZE_MASK));
        Writer_WriteByte(msg, (needInflate? TRMF_DEFLATED : 0) | (size >> TRMF_SIZE_SHIFT));
    }
    else if(size <= MAX_SIZE_LARGE)
    {
        assert(needInflate);
        Writer_WriteByte(msg, TRMF_CONTINUE | (size & TRMF_SIZE_MASK));
        Writer_WriteByte(msg, TRMF_CONTINUE | ((size >> TRMF_SIZE_SHIFT) & TRMF_SIZE_MASK));
        Writer_WriteByte(msg, size >> (2*TRMF_SIZE_SHIFT));
    }
    else
    {
        // Not supported.
        assert(false);
    }

    // The payload.
    Writer_Write(msg, data, size);

    msgSize = Writer_Size(msg);
    Writer_Delete(msg);
    return msgSize;
}

void Protocol_Send(void *data, size_t size, nodeid_t destination)
{
    if(size == 0 || !N_GetNodeSocket(destination) || !N_HasNodeJoined(destination))
        return;

    LegacyNetwork_Send(N_GetNodeSocket(destination), data, size);

#if 0
    int result = 0;
    size_t transmissionSize = 0;
    size_t huffSize = 0;
    uint8_t* huffData = 0;

    if(size == 0 || !N_GetNodeSocket(destination) || !N_HasNodeJoined(destination))
        return;

    if(size > DDMAXINT)
    {
        Con_Error("Protocol_Send: Trying to send an oversized data buffer.\n"
                  "  Attempted size is %u bytes.\n", (unsigned long) size);
    }

#ifdef _DEBUG
    Monitor_Add(data, size);
#endif

    // Let's first see if the encoded contents are under 128 bytes
    // as Huffman codes.
    if(size <= MAX_HUFFMAN_INPUT_SIZE) // Potentially short enough.
    {
        huffData = Huffman_Encode(data, size, &huffSize);
        if(huffSize <= MAX_SIZE_SMALL)
        {
            // We can use this: set up a small-sized transmission.
            transmissionSize = prepareTransmission(huffData, huffSize, false);
        }
    }

    if(!transmissionSize) // Let's deflate, then.
    {
        /// @todo Messages broadcasted to multiple recipients are separately
        /// compressed for each TCP send -- should do only one compression
        /// per message.

        size_t compSize = 0;
        uint8_t* compData = ZipFile_CompressAtLevel(data, size, &compSize,
            size < 2*MAX_SIZE_MEDIUM? 6 /*default*/ : 9 /*best*/);
        if(!compData)
        {
            M_Free(huffData);
            Con_Error("Protocol_Send: Failed to compress transmission.\n");
        }
        if(compSize > MAX_SIZE_LARGE)
        {
            M_Free(huffData);
            M_Free(compData);
            Con_Error("Protocol_Send: Compressed payload is too large (%u bytes).\n", compSize);
        }

        // We can choose the smallest compression.
        if(huffData && huffSize <= compSize && huffSize <= MAX_SIZE_MEDIUM)
        {
            // Huffman yielded smaller payload.
            transmissionSize = prepareTransmission(huffData, huffSize, false);
        }
        else
        {
            // Use the deflated payload.
            transmissionSize = prepareTransmission(compData, compSize, true /*deflated*/);
        }
        M_Free(compData);
    }

    M_Free(huffData);

    // Send the data over the socket.
    result = SDLNet_TCP_Send((TCPsocket)N_GetNodeSocket(destination),
                             transmissionBuffer, transmissionSize);
    if(result < 0 || (size_t) result != transmissionSize)
    {
        perror("Socket error");
    }

    // Statistics.
    N_AddSentBytes(transmissionSize);
#endif
}
