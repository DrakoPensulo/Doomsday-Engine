/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 * ext.c: OpenGL Extensions
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

// HEADER FILES ------------------------------------------------------------

#include "dropengl.h"

// MACROS ------------------------------------------------------------------

#ifdef WIN32
#   define GETPROC(x)   x = (void*)wglGetProcAddress(#x)
#elif defined(UNIX)
#   define GETPROC(x)   x = SDL_GL_GetProcAddress(#x)
#endif

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     extMultiTex;
int     extBlendSub;
int     extTexEnvComb;
int     extNvTexEnvComb;
int     extAtiTexEnvComb;
int     extAniso;
int     extVSync;
int     extLockArray;
int     extGenMip;
int     extS3TC;

#ifdef WIN32
PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTextureARB;
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * @return              Non-zero iff the extension string is found. This
 *                      function is based on the method used by David Blythe
 *                      and Tom McReynolds in the book "Advanced Graphics
 *                      Programming Using OpenGL" ISBN: 1-55860-659-9.
 */
static int queryExtension(const char *name)
{
    const GLubyte *extensions = NULL;
    const GLubyte *start;
    GLubyte    *c, *terminator;

    // Extension names should not have spaces.
    c = (GLubyte *) strchr(name, ' ');
    if(c || *name == '\0')
        return false;

    extensions = glGetString(GL_EXTENSIONS);

    // It takes a bit of care to be fool-proof about parsing the
    // OpenGL extensions string. Don't be fooled by sub-strings, etc.
    start = extensions;
    for(;;)
    {
        c = (GLubyte *) strstr((const char *) start, name);
        if(!c)
            break;

        terminator = c + strlen(name);
        if(c == start || *(c - 1) == ' ')
            if(*terminator == ' ' || *terminator == '\0')
            {
                return true;
            }
        start = terminator;
    }

    return false;
}

static int query(const char *ext, int *var)
{
    int         result = DGL_FALSE;

    if(ext)
    {
        Con_Message("Checking OpenGL extension: %s\n", ext);
        result = (queryExtension(ext)? DGL_TRUE : DGL_FALSE);
        if(var)
            *var = result;
    }

    return result;
}

/**
 * Pre: A rendering context must be aquired and made current before this is
 * called.
 */
void DG_InitExtensions(void)
{
    int         i;

    if(query("GL_EXT_compiled_vertex_array", &extLockArray))
    {
#ifdef WIN32
        GETPROC(glLockArraysEXT);
        GETPROC(glUnlockArraysEXT);
#endif
    }

    query("GL_EXT_paletted_texture", &palExtAvailable);
    query("GL_EXT_shared_texture_palette", &sharedPalExtAvailable);
    query("GL_EXT_texture_filter_anisotropic", &extAniso);

    if(query("WGL_EXT_extensions_string", &extVSync))
    {
#ifdef WIN32
        GETPROC(wglSwapIntervalEXT);
#endif
    }

    // EXT_blend_subtract
    if(query("GL_EXT_blend_subtract", &extBlendSub))
    {
#ifdef WIN32
        GETPROC(glBlendEquationEXT);
#endif
    }

    // ARB_texture_env_combine
    if(!query("GL_ARB_texture_env_combine", &extTexEnvComb))
    {
        // Try the older EXT_texture_env_combine (identical to ARB).
        query("GL_EXT_texture_env_combine", &extTexEnvComb);
    }

    // NV_texture_env_combine4
    query("GL_NV_texture_env_combine4", &extNvTexEnvComb);

    // ATI_texture_env_combine3
    query("GL_ATI_texture_env_combine3", &extAtiTexEnvComb);

    // Texture compression.
    query("GL_EXT_texture_compression_s3tc", &extS3TC);
    // On by default if we have it.
    glGetError();
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, (GLint*) &i);
    if(i && glGetError() == GL_NO_ERROR)
    {
        useCompr = DGL_TRUE;
        Con_Message("OpenGL: Texture compression (%i formats).\n", i);
    }

    if(ArgExists("-notexcomp"))
    {
        useCompr = DGL_FALSE;
    }

#ifdef USE_MULTITEXTURE
    // ARB_multitexture
    if(query("GL_ARB_multitexture", &extMultiTex))
    {
#  ifdef WIN32
        // Get the function pointers.
        GETPROC(glClientActiveTextureARB);
        GETPROC(glActiveTextureARB);
        GETPROC(glMultiTexCoord2fARB);
        GETPROC(glMultiTexCoord2fvARB);
#  endif
    }
#endif

    // Automatic mipmap generation.
    if(!ArgExists("-nosgm") && query("GL_SGIS_generate_mipmap", &extGenMip))
    {
        // Use nice quality, please.
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
    }
}
