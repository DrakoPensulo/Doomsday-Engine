/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * dd_wad.h: WAD Files and Data Lump Cache
 */

#ifndef __DOOMSDAY_WAD_H__
#define __DOOMSDAY_WAD_H__

#include "sys_file.h"
#include "con_decl.h"

#define RECORD_FILENAMELEN  256

// File record flags.
#define FRF_RUNTIME     0x1        // Loaded at runtime (for reset).

#define AUXILIARY_BASE  100000000

enum                               // Lump Grouping Tags
{
    LGT_NONE = 0,
    LGT_FLATS,
    LGT_SPRITES,
    NUM_LGTAGS
};

typedef struct {
    char            filename[RECORD_FILENAMELEN];   // Full filename (every '\' -> '/').
    int             numlumps;      // Number of lumps.
    int             flags;
    DFILE          *handle;        // File handle.
    char            iwad;
} filerecord_t;

typedef struct {
    char            name[9];       // End in \0.
    DFILE          *handle;
    int             position;
    size_t          size;
    int             sent;
    char            group;         // Lump grouping tag (LGT_*).
} lumpinfo_t;

extern char    *defaultWads;       // A list of wad names, whitespace in between (in .cfg).
extern lumpinfo_t *lumpinfo;
extern int      numlumps;

void            DD_RegisterVFS(void);

void            W_InitMultipleFiles(char **filenames);
void            W_EndStartup(void);
int             W_CheckNumForName(const char *name);
int             W_GetNumForName(const char *name);
size_t          W_LumpLength(int lump);
const char     *W_LumpName(int lump);
void            W_ReadLump(int lump, void *dest);
void            W_ReadLumpSection(int lump, void *dest, int startoffset,
                                  size_t length);
void           *W_CacheLumpNum(int lump, int tag);
void           *W_CacheLumpName(char *name, int tag);
boolean         W_AddFile(const char *filename, boolean allowDuplicate);
boolean         W_RemoveFile(char *filename);
void            W_Reset();
int             W_OpenAuxiliary(const char *filename);
void            W_ChangeCacheTag(int lump, int tag);
void            W_CheckIWAD(void);
boolean         W_IsFromIWAD(int lump);
const char     *W_LumpSourceFile(int lump);
unsigned int    W_CRCNumberForRecord(int idx);
unsigned int    W_CRCNumber(void);
void            W_GetIWADFileName(char *buf, int bufSize);
void            W_GetPWADFileNames(char *buf, int bufSize, char separator);
void            W_PrintMapList(void);

#endif
