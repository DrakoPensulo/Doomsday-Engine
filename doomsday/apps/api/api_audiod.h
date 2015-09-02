/** @file api_audiod.h  Audio driver interface.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_API_AUDIO_DRIVER_INTERFACE_H
#define CLIENT_API_AUDIO_DRIVER_INTERFACE_H

/**
 * @defgroup audio Audio
 */
///@{

/**
 * Audio driver properties.
 */
enum
{
    AUDIOP_IDENTIFIER,         ///< Symbolic driver identifier (stored persistently, in Config).
    AUDIOP_NAME,               ///< Human-friendly textual name (for user presentation, in the UI).
    AUDIOP_SOUNDFONT_FILENAME  ///< Music sound font (file name and path).
};

typedef enum {
    AUDIO_INONE,
    AUDIO_ISFX,
    AUDIO_IMUSIC,
    AUDIO_ICD,
    AUDIO_IMUSIC_OR_ICD
} audiointerfacetype_t;

typedef struct audiodriver_s {
    int (*Init) (void);
    void (*Shutdown) (void);
    void (*Event) (int type);
    int (*Get) (int prop, void *ptr);
    int (*Set) (int prop, void const *ptr);
} audiodriver_t;

///@}

#endif  // CLIENT_API_AUDIO_DRIVER_INTERFACE_H
