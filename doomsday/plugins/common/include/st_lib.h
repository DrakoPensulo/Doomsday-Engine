/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * The status bar widget code.
 * Compiles for jDoom/jHeretic/jHexen
 */

#ifndef __COMMON_STLIB__
#define __COMMON_STLIB__

#include "hu_stuff.h"

//
// Background and foreground screen numbers
//
#define BG 4
#define FG 0

// Patch used for '-' in the status bar
#if __DOOM64TC__
# define MINUSPATCH "FONTB046"
#elif __JDOOM__
# define MINUSPATCH "STTMINUS"
#else
# define MINUSPATCH "FONTB13"
#endif

//
// Typedefs of widgets
//

// Number widget

typedef struct {
    // upper right-hand corner
    //  of the number (right-justified)
    int             x;
    int             y;

    // max # of digits in number
    int             width;

    // last number value
    int             oldnum;

    // pointer to alpha
    float          *alpha;

    // pointer to current value
    int            *num;

    // pointer to boolean stating
    //  whether to update number
    boolean        *on;

    // list of patches for 0-9
    dpatch_t       *p;

    // user data
    int             data;

} st_number_t;

// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
typedef struct {
    // number information
    st_number_t     n;

    // percent sign graphic
    dpatch_t       *p;

} st_percent_t;

// Multiple Icon widget
typedef struct {
    // center-justified location of icons
    int             x;
    int             y;

    // last icon number
    int             oldinum;

    // pointer to current icon
    int            *inum;

    // pointer to alpha
    float          *alpha;

    // pointer to boolean stating
    //  whether to update icon
    boolean        *on;

    // list of icons
    dpatch_t       *p;

    // user data
    int             data;

} st_multicon_t;

// Binary Icon widget

typedef struct {
    // center-justified location of icon
    int             x;
    int             y;

    // last icon value
    int             oldval;

    // pointer to current icon status
    boolean        *val;

    // pointer to alpha
    float          *alpha;

    // pointer to boolean
    //  stating whether to update icon
    boolean        *on;

    dpatch_t       *p;             // icon
    int             data;          // user data

} st_binicon_t;

//
// Widget creation, access, and update routines
//

// Initializes widget library.
// More precisely, initialize STMINUS,
//  everything else is done somewhere else.
//
void            STlib_init(void);

// Number widget routines
void            STlib_initNum(st_number_t * n, int x, int y, dpatch_t * pl,
                              int *num, boolean *on, int width, float *alpha);

void            STlib_updateNum(st_number_t * n, boolean refresh);

// Percent widget routines
void            STlib_initPercent(st_percent_t * p, int x, int y,
                                  dpatch_t * pl, int *num, boolean *on,
                                  dpatch_t * percent, float *alpha);

void            STlib_updatePercent(st_percent_t * per, int refresh);

// Multiple Icon widget routines
void            STlib_initMultIcon(st_multicon_t * mi, int x, int y,
                                   dpatch_t * il, int *inum, boolean *on, float *alpha);

void            STlib_updateMultIcon(st_multicon_t * mi, boolean refresh);

// Binary Icon widget routines

void            STlib_initBinIcon(st_binicon_t * b, int x, int y, dpatch_t * i,
                                  boolean *val, boolean *on, int d, float *alpha);

void            STlib_updateBinIcon(st_binicon_t * bi, boolean refresh);

#endif
