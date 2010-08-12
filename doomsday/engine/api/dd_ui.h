/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_API_GUI_H
#define LIBDENG_API_GUI_H

/**
 * @defgroup gui GUI System
 */

#include "dd_animator.h"
#include "dd_compositefont.h"
#include "dd_vectorgraphic.h"

typedef ident_t fi_objectid_t;

#define FI_NAME_MAX_LENGTH          32
typedef char fi_name_t[FI_NAME_MAX_LENGTH];
typedef fi_name_t fi_objectname_t;

typedef enum {
    FI_NONE,
    FI_TEXT,
    FI_PIC
} fi_obtype_e;

struct fi_object_s;
struct fi_page_s;

/**
 * Base fi_objects_t elements. All objects MUST use this as their basis.
 *
 * @ingroup video
 */
#define FIOBJECT_BASE_ELEMENTS() \
    fi_obtype_e     type; /* Type of the object. */ \
    int             group; \
    struct { \
        char looping:1; /* Animation will loop. */ \
    } flags; \
    boolean         animComplete; /* Animation finished (or repeated). */ \
    fi_objectid_t   id; /* Unique id of the object. */ \
    fi_objectname_t name; /* Nice name. */ \
    animatorvector3_t pos; \
    animator_t      angle; \
    animatorvector3_t scale; \
    void          (*drawer) (struct fi_object_s*, const float offset[3]); \
    void          (*thinker) (struct fi_object_s* obj);

struct fi_object_s* FI_NewObject(fi_obtype_e type, const char* name);
void FI_DeleteObject(struct fi_object_s* obj);
struct fi_object_s* FI_Object(fi_objectid_t id);

/**
 * A page is an aggregate visual/visual container.
 *
 * @ingroup video
 */
typedef struct {
    struct fi_object_s** vector;
    uint size;
} fi_object_collection_t;

typedef struct fi_page_s {
    struct fi_page_flags_s {
        char hidden:1; /// Currently hidden (not drawn).
        char paused:1; /// Currently paused (does not tic).
    } flags;

    /// Child visuals (objects) visible on this page.
    /// \note Unlike de::Visual the children are not owned by the page.
    fi_object_collection_t _objects;

    /// Offset the world origin.
    animatorvector3_t _offset;

    void (*drawer) (struct fi_page_s* page);
    void (*ticker) (struct fi_page_s* page, timespan_t ticLength);

    /// Pointer to the previous page, if any.
    struct fi_page_s* previous;

    struct fi_page_background_s {
        struct material_s* material;
        DGLuint tex;
        animatorvector4_t topColor;
        animatorvector4_t bottomColor;
    } _bg;

    animatorvector4_t _filter;
    animatorvector3_t _textColor[9];

    uint _timer;
} fi_page_t;

fi_page_t* FI_NewPage(fi_page_t* prevPage);
void FI_DeletePage(fi_page_t* page);

/// Draws the page if not hidden.
void FIPage_Drawer(fi_page_t* page);

/// Tic the page if not paused.
void FIPage_Ticker(fi_page_t* page, timespan_t ticLength);

/// Adds a UI object to the page if not already present.
struct fi_object_s* FIPage_AddObject(fi_page_t* page, struct fi_object_s* obj);

/// Removes a UI object from the page if present.
struct fi_object_s* FIPage_RemoveObject(fi_page_t* page, struct fi_object_s* obj);

/// Is the UI object present on the page?
boolean FIPage_HasObject(fi_page_t* page, struct fi_object_s* obj);

/// Current background Material.
struct material_s* FIPage_BackgroundMaterial(fi_page_t* page);

/// Sets the 'is-visible' state.
void FIPage_MakeVisible(fi_page_t* page, boolean yes);

/// Sets the 'is-paused' state.
void FIPage_Pause(fi_page_t* page, boolean yes);

/// Sets the background Material.
void FIPage_SetBackgroundMaterial(fi_page_t* page, struct material_s* mat);

/// Sets the background top color.
void FIPage_SetBackgroundTopColor(fi_page_t* page, float red, float green, float blue, int steps);

/// Sets the background top color and alpha.
void FIPage_SetBackgroundTopColorAndAlpha(fi_page_t* page, float red, float green, float blue, float alpha, int steps);

/// Sets the background bottom color.
void FIPage_SetBackgroundBottomColor(fi_page_t* page, float red, float green, float blue, int steps);

/// Sets the background bottom color and alpha.
void FIPage_SetBackgroundBottomColorAndAlpha(fi_page_t* page, float red, float green, float blue, float alpha, int steps);

/// Sets the x-axis component of the world offset.
void FIPage_SetOffsetX(fi_page_t* page, float x, int steps);

/// Sets the y-axis component of the world offset.
void FIPage_SetOffsetY(fi_page_t* page, float y, int steps);

/// Sets the world offset.
void FIPage_SetOffsetXYZ(fi_page_t* page, float x, float y, float z, int steps);

/// Sets the filter color and alpha.
void FIPage_SetFilterColorAndAlpha(fi_page_t* page, float red, float green, float blue, float alpha, int steps);

/// Sets a predefined color.
void FIPage_SetPredefinedColor(fi_page_t* page, uint idx, float red, float green, float blue, int steps);

/**
 * Rectangle/Image sequence object.
 *
 * @ingroup video
 */
typedef struct fidata_pic_frame_s {
    int tics;
    enum {
        PFT_MATERIAL,
        PFT_PATCH,
        PFT_RAW, /// "Raw" graphic or PCX lump.
        PFT_XIMAGE /// External graphics resource.
    } type;
    struct fidata_pic_frame_flags_s {
        char flip:1;
    } flags;
    union {
        struct material_s* material;
        patchid_t patch;
        lumpnum_t lump;
        DGLuint tex;
    } texRef;
    short sound;
} fidata_pic_frame_t;

typedef struct fidata_pic_s {
    FIOBJECT_BASE_ELEMENTS()
    int tics;
    uint curFrame;
    fidata_pic_frame_t** frames;
    uint numFrames;

    animatorvector4_t color;

    /// For rectangle-objects.
    animatorvector4_t otherColor;
    animatorvector4_t edgeColor;
    animatorvector4_t otherEdgeColor;
} fidata_pic_t;

void FIData_PicThink(struct fi_object_s* pic);
void FIData_PicDraw(struct fi_object_s* pic, const float offset[3]);
uint FIData_PicAppendFrame(struct fi_object_s* pic, int type, int tics, void* texRef, short sound, boolean flagFlipH);
void FIData_PicClearAnimation(struct fi_object_s* pic);

/**
 * Text object.
 *
 * @ingroup video
 */
typedef struct fidata_text_s {
    FIOBJECT_BASE_ELEMENTS()
    animatorvector4_t color;
    short textFlags; /// @see drawTextFlags
    int scrollWait, scrollTimer; /// Automatic scrolling upwards.
    size_t cursorPos;
    int wait, timer;
    float lineHeight;
    compositefontid_t font;
    char* text;
} fidata_text_t;

void FIData_TextThink(struct fi_object_s* text);
void FIData_TextDraw(struct fi_object_s* text, const float offset[3]);
void FIData_TextCopy(struct fi_object_s* text, const char* str);

/**
 * @return Length of the current text as a counter.
 */
size_t FIData_TextLength(struct fi_object_s* text);

#endif /* LIBDENG_API_GUI_H */
