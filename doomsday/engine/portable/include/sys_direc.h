/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * sys_direc.h: Directory Utilities
 */

#ifndef __DOOMSDAY_DIREC_H__
#define __DOOMSDAY_DIREC_H__

#include "dd_types.h"

void            Dir_GetDir(directory_t * dir);
int             Dir_ChDir(directory_t * dir);
void            Dir_FileDir(const char *str, directory_t * dir);
void            Dir_FileName(const char *str, char *name);
void            Dir_MakeDir(const char *path, directory_t * dir);
void            Dir_FileID(const char *str, byte identifier[16]);
void            Dir_FixSlashes(char *path);
void            Dir_ValidDir(char *str);
boolean         Dir_IsEqual(directory_t * a, directory_t * b);
int             Dir_IsAbsolute(const char *str);
void            Dir_MakeAbsolute(char *path);

#endif
