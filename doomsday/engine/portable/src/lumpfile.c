/**\file lumpfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "lumpdirectory.h"
#include "lumpfile.h"

LumpFile* LumpFile_New(DFile* file, const char* path, const LumpInfo* info)
{
    LumpFile* lump = (LumpFile*)malloc(sizeof *lump);
    if(!lump) Con_Error("LumpFile::Construct: Failed on allocation of %lu bytes for new LumpFile.",
                (unsigned long) sizeof *lump);

    AbstractFile_Init((abstractfile_t*)lump, FT_LUMPFILE, path, file, info);
    lump->_cacheData = NULL;
    return lump;
}

void LumpFile_Delete(LumpFile* lump)
{
    assert(lump);
    F_ReleaseFile((abstractfile_t*)lump);
    LumpFile_ClearLumpCache(lump);
    AbstractFile_Destroy((abstractfile_t*)lump);
    free(lump);
}

int LumpFile_PublishLumpsToDirectory(LumpFile* lump, LumpDirectory* directory)
{
    assert(lump && directory);
    // Insert the lumps into their rightful places in the directory.
    LumpDirectory_CatalogLumps(directory, (abstractfile_t*)lump, 0, 1);
    return 1;
}

ddstring_t* LumpFile_ComposeLumpPath(LumpFile* lump, int lumpIdx, char delimiter)
{
    const LumpInfo* info = AbstractFile_Info((abstractfile_t*)lump);
    // Our container knows our path.
    return F_ComposeLumpPath2(info->container, info->lumpIdx, delimiter);
}

const LumpInfo* LumpFile_LumpInfo(LumpFile* lump, int lumpIdx)
{
    assert(lump);
    /// Lump files are special cases for this *is* the lump.
    return AbstractFile_Info((abstractfile_t*)lump);
}

void LumpFile_ClearLumpCache(LumpFile* lump)
{
    assert(lump);
    if(lump->_cacheData)
    {
        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(lump->_cacheData) < PU_MAP)
            Z_ChangeTag(lump->_cacheData, PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(lump->_cacheData, (void*) 0x2);
    }
}

size_t LumpFile_ReadLumpSection2(LumpFile* lump, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    const LumpInfo* info = LumpFile_LumpInfo(lump, lumpIdx);
    size_t readBytes;

    VERBOSE2(
        Con_Printf("LumpFile::ReadLumpSection: \"%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)lump))),
                (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length)
    )

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && lump->_cacheData)
    {
        boolean isCached = (NULL != lump->_cacheData);
        void** cachePtr = (void**)&lump->_cacheData;
        if(isCached)
        {
            VERBOSE2( Con_Printf(" from cache\n") )
            readBytes = MIN_OF(info->size, length);
            memcpy(buffer, (char*)*cachePtr + startOffset, readBytes);
            return readBytes;
        }
    }

    VERBOSE2( Con_Printf("\n") )
    DFile_Seek(lump->_base._file, startOffset, SEEK_SET);
    readBytes = DFile_Read(lump->_base._file, buffer, length);
    if(readBytes < length)
    {
        /// \todo Do not do this here.
        Con_Error("LumpFile::ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpIdx);
    }
    return readBytes;
}

size_t LumpFile_ReadLumpSection(LumpFile* lump, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return LumpFile_ReadLumpSection2(lump, lumpIdx, buffer, startOffset, length, true);
}

size_t LumpFile_ReadLump2(LumpFile* lump, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    const LumpInfo* info = LumpFile_LumpInfo(lump, lumpIdx);
    if(!info) return 0;
    return LumpFile_ReadLumpSection2(lump, lumpIdx, buffer, 0, info->size, tryCache);
}

size_t LumpFile_ReadLump(LumpFile* lump, int lumpIdx, uint8_t* buffer)
{
    return LumpFile_ReadLump2(lump, lumpIdx, buffer, true);
}

const uint8_t* LumpFile_CacheLump(LumpFile* lump, int lumpIdx, int tag)
{
    const LumpInfo* info = LumpFile_LumpInfo(lump, lumpIdx);
    boolean isCached = (NULL != lump->_cacheData);
    void** cachePtr = (void**)&lump->_cacheData;

    VERBOSE2(
        Con_Printf("LumpFile::CacheLump: \"%s\" (%lu bytes%s) %s\n",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)lump))),
                (unsigned long) info->size, (info->compressedSize != info->size? ", compressed" : ""),
                isCached? "hit":"miss")
    )

    if(!isCached)
    {
        uint8_t* ptr;
        assert(info);
        ptr = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(!ptr)
            Con_Error("LumpFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        LumpFile_ReadLump2(lump, lumpIdx, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
}

void LumpFile_ChangeLumpCacheTag(LumpFile* lump, int lumpIdx, int tag)
{
    boolean isCached = (NULL != lump->_cacheData);
    void** cachePtr = (void**)&lump->_cacheData;

    if(isCached)
    {
        VERBOSE2(
            const LumpInfo* info = LumpFile_LumpInfo(lump, lumpIdx);
            Con_Printf("LumpFile::ChangeLumpCacheTag: \"%s\" tag=%i\n",
                       F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)lump))), tag)
        )

        Z_ChangeTag2(*cachePtr, tag);
    }
}

int LumpFile_LumpCount(LumpFile* lump)
{
    return 1; // Always.
}
