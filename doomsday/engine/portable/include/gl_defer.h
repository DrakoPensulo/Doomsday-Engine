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
 * gl_defer.h: Deferred GL Tasks
 */

#ifndef __DOOMSDAY_GRAPHICS_DEREF_H__
#define __DOOMSDAY_GRAPHICS_DEREF_H__

/**
 * Defines the content of a DGL texture. Used when creating textures either 
 * immediately or in deferred mode (when busy).
 */
typedef struct texturecontent_s {
    DGLuint name;
    void* buffer;
    int bufferSize;
    int format;
    int width;
    int height;
    int minFilter;
    int magFilter;
    int anisoFilter;
    int wrap[2];
    int grayMipmap;
    int flags;
} texturecontent_t;   

// Flags for texture content.
#define TXCF_NO_COMPRESSION             0x1
#define TXCF_MIPMAP                     0x2
#define TXCF_GET_GRAY_MIPMAP            0x4
#define TXCF_EASY_UPLOAD                0x8
#define TXCF_UPLOAD_ARG_ALPHACHANNEL    0x10
#define TXCF_UPLOAD_ARG_RGBDATA         0x20
#define TXCF_UPLOAD_ARG_NOSTRETCH       0x40
#define TXCF_UPLOAD_ARG_NOSMARTFILTER   0x80
#define TXCF_NEVER_DEFER                0x100

void            GL_InitDeferred(void);
void            GL_ShutdownDeferred(void);
void            GL_UploadDeferredContent(uint timeOutMilliSeconds);
int             GL_GetDeferredCount(void);
void            GL_InitTextureContent(texturecontent_t *content);
DGLuint         GL_NewTexture(texturecontent_t *content);                              
DGLuint         GL_NewTextureWithParams(int format, int width, int height, void* pixels, 
                                        int flags);
DGLuint         GL_NewTextureWithParams2(int format, int width, int height, void* pixels, 
                                         int flags, int minFilter, int magFilter, int anisoFilter, 
                                         int wrapS, int wrapT);

#endif
