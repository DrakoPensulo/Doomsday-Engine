/* Generated by .\..\..\engine\scripts\makedmt.py */

#ifndef __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__
#define __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__

#include "p_mapdata.h"

// Each Sector and SideDef has an origin in the world (used for distance based delta queuing)
typedef struct origin_s {
    float               pos[2];
} origin_t;

#define LO_prev     link[0]
#define LO_next     link[1]

typedef struct shadowvert_s {
    float           inner[2];
    float           extended[2];
} shadowvert_t;

typedef struct lineowner_s {
    struct linedef_s *lineDef;
    struct lineowner_s *link[2];    // {prev, next} (i.e. {anticlk, clk}).
    binangle_t      angle;          // between this and next clockwise.
    shadowvert_t    shadowOffsets;
} lineowner_t;

#define V_pos                   v.pos

typedef struct mvertex_s {
    // Vertex index. Always valid after loading and pruning of unused
    // vertices has occurred.
    int         index;

    // Reference count. When building normal node info, unused vertices
    // will be pruned.
    int         refCount;

    // Usually NULL, unless this vertex occupies the same location as a
    // previous vertex. Only used during the pruning phase.
    struct vertex_s *equiv;

    struct edgetip_s *tipSet; // Set of wall_tips.

// Final data.
    double      pos[2];
} mvertex_t;

typedef struct vertex_s {
    runtime_mapdata_header_t header;
    unsigned int        numLineOwners; // Number of line owners.
    lineowner_t*        lineOwners;    // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    fvertex_t           v;
    mvertex_t           buildData;
} Vertex;

// Helper macros for accessing hedge data elements.
#define FRONT 0
#define BACK  1

#define HE_v(n)                 v[(n)? 1:0]
#define HE_vpos(n)              HE_v(n)->V_pos

#define HE_v1                   HE_v(0)
#define HE_v1pos                HE_v(0)->V_pos

#define HE_v2                   HE_v(1)
#define HE_v2pos                HE_v(1)->V_pos

#define HE_sector(n)            sec[(n)? 1:0]
#define HE_frontsector          HE_sector(FRONT)
#define HE_backsector           HE_sector(BACK)

#define HEDGE_SIDEDEF(s)          ((s)->lineDef->sideDefs[(s)->side])

// HEdge flags
#define HEDGEF_POLYOBJ            0x1 // HEdge is part of a poly object.

// HEdge frame flags
#define HEDGEINF_FACINGFRONT      0x0001

typedef struct hedge_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v[2];          // [Start, End] of the segment.
    struct linedef_s*   lineDef;
    struct sector_s*    sec[2];
    struct bspleaf_s*   bspLeaf;
    struct hedge_s*     twin;
    angle_t             angle;
    byte                side;          // 0=front, 1=back
    byte                flags;
    float               length;        // Accurate length of the segment (v1 -> v2).
    float               offset;
    biassurface_t*      bsuf[3];       // 0=middle, 1=top, 2=bottom
    short               frameFlags;
} HEdge;

#define BLF_MIDPOINT         0x80    // Midpoint is tri-fan centre.

typedef struct bspleaf_s {
    runtime_mapdata_header_t header;
    unsigned int        hedgeCount;
    struct hedge_s**    hedges;        // [hedgeCount] size.
    struct polyobj_s*   polyObj;       // NULL, if there is no polyobj.
    struct sector_s*    sector;
    int                 addSpriteCount; // frame number of last R_AddSprites
    unsigned int        inSectorID;
    int                 flags;
    int                 validCount;
    unsigned int        reverb[NUM_REVERB_DATA];
    AABoxf              aaBox;         // Min and max points.
    float               worldGridOffset[2]; // Offset to align the top left of the bBox to the world grid.
    fvertex_t           midPoint;      // Center of vertices.
    unsigned short      numVertices;
    struct fvertex_s**  vertices;      // [numvertices] size
    struct shadowlink_s* shadows;
    struct biassurface_s** bsuf;       // [sector->planeCount] size.
} BspLeaf;

typedef enum {
    MEC_UNKNOWN = -1,
    MEC_FIRST = 0,
    MEC_METAL = MEC_FIRST,
    MEC_ROCK,
    MEC_WOOD,
    MEC_CLOTH,
    NUM_MATERIAL_ENV_CLASSES
} material_env_class_t;

#define VALID_MATERIAL_ENV_CLASS(v) ((v) >= MEC_FIRST && (v) < NUM_MATERIAL_ENV_CLASSES)

struct material_variantlist_node_s;

typedef struct material_s {
    runtime_mapdata_header_t header;
    struct ded_material_s* _def;
    struct material_variantlist_node_s* _variants;
    material_env_class_t _envClass;    // Environmental sound class.
    materialid_t        _primaryBind;  // Unique identifier of the MaterialBind associated with this Material or @c NULL if not bound.
    Size2*              _size;         // Logical dimensions in world-space units.
    short               _flags;        // @see materialFlags
    boolean             _inAnimGroup;  // @c true if belongs to some animgroup.
    boolean             _isCustom;
    struct texture_s*   _detailTex;
    float               _detailScale;
    float               _detailStrength;
    struct texture_s*   _shinyTex;
    blendmode_t         _shinyBlendmode;
    float               _shinyMinColor[3];
    float               _shinyStrength;
    struct texture_s*   _shinyMaskTex;
    byte                _prepared;
} material_t;

// Internal surface flags:
#define SUIF_PVIS             0x0001
#define SUIF_FIX_MISSING_MATERIAL 0x0002 // Current texture is a fix replacement
                                     // (not sent to clients, returned via DMU etc).
#define SUIF_BLEND            0x0004 // Surface possibly has a blended texture.
#define SUIF_NO_RADIO         0x0008 // No fakeradio for this surface.

#define SUIF_UPDATE_FLAG_MASK 0xff00
#define SUIF_UPDATE_DECORATIONS 0x8000

typedef struct surfacedecor_s {
    float               pos[3]; // World coordinates of the decoration.
    BspLeaf*		bspLeaf;
    const struct ded_decorlight_s* def;
} surfacedecor_t;

typedef struct surface_s {
    runtime_mapdata_header_t header;
    void*               owner;         // Either @c DMU_SIDEDEF, or @c DMU_PLANE
    int                 flags;         // SUF_ flags
    int                 oldFlags;
    material_t*         material;
    blendmode_t         blendMode;
    float               tangent[3];
    float               bitangent[3];
    float               normal[3];
    float               offset[2];     // [X, Y] Planar offset to surface material origin.
    float               oldOffset[2][2];
    float               visOffset[2];
    float               visOffsetDelta[2];
    float               rgba[4];       // Surface color tint
    short               inFlags;       // SUIF_* flags
    unsigned int        numDecorations;
    surfacedecor_t      *decorations;
} Surface;

typedef enum {
    PLN_FLOOR,
    PLN_CEILING,
    PLN_MID,
    NUM_PLANE_TYPES
} planetype_t;

#define PS_tangent              surface.tangent
#define PS_bitangent            surface.bitangent
#define PS_normal               surface.normal
#define PS_material             surface.material
#define PS_offset               surface.offset
#define PS_visoffset            surface.visOffset
#define PS_rgba                 surface.rgba
#define PS_flags                surface.flags
#define PS_inflags              surface.inFlags

typedef struct plane_s {
    runtime_mapdata_header_t header;
    ddmobj_base_t       origin;
    struct sector_s*    sector;        // Owner of the plane (temp)
    Surface             surface;
    float               height;        // Current height
    float               oldHeight[2];
    float               target;        // Target height
    float               speed;         // Move speed
    float               visHeight;     // Visible plane height (smoothed)
    float               visHeightDelta;
    planetype_t         type;          // PLN_* type.
    int                 planeID;
} Plane;

// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planetangent(n)      SP_plane(n)->surface.tangent
#define SP_planebitangent(n)    SP_plane(n)->surface.bitangent
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planematerial(n)     SP_plane(n)->surface.material
#define SP_planeoffset(n)       SP_plane(n)->surface.offset
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planeorigin(n)       SP_plane(n)->origin
#define SP_planevisheight(n)    SP_plane(n)->visHeight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceiltangent          SP_planetangent(PLN_CEILING)
#define SP_ceilbitangent        SP_planebitangent(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceilmaterial         SP_planematerial(PLN_CEILING)
#define SP_ceiloffset           SP_planeoffset(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceilorigin           SP_planeorigin(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floortangent         SP_planetangent(PLN_FLOOR)
#define SP_floorbitangent       SP_planebitangent(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floormaterial        SP_planematerial(PLN_FLOOR)
#define SP_flooroffset          SP_planeoffset(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floororigin          SP_planeorigin(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)             skyFix[(n)]
#define S_floorskyfix           S_skyfix(PLN_FLOOR)
#define S_ceilskyfix            S_skyfix(PLN_CEILING)

// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_UNCLOSED       0x1     // An unclosed sector (some sort of fancy hack).

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int         index;

    // Suppress superfluous mini warnings.
    int         warnedFacing;
    int			refCount;
} msector_t;

typedef struct sector_s {
    runtime_mapdata_header_t header;
    int                 frameFlags;
    int                 validCount;    // if == validCount, already checked.
    int                 flags;
    AABoxf              aaBox;         // Bounding box for the sector.
    float               roughArea;    // Rough approximation of sector area.
    float               lightLevel;
    float               oldLightLevel;
    float               rgb[3];
    float               oldRGB[3];
    struct mobj_s*      mobjList;      // List of mobjs in the sector.
    unsigned int        lineDefCount;
    struct linedef_s**  lineDefs;      // [lineDefCount+1] size.
    unsigned int        bspLeafCount;
    struct bspleaf_s**  bspLeafs;     // [bspLeafCount+1] size.
    unsigned int        numReverbBspLeafAttributors;
    struct bspleaf_s**  reverbBspLeafs;  // [numReverbBspLeafAttributors] size.
    ddmobj_base_t       origin;
    unsigned int        planeCount;
    struct plane_s**    planes;        // [planeCount+1] size.
    unsigned int        blockCount;    // Number of gridblocks in the sector.
    unsigned int        changedBlockCount; // Number of blocks to mark changed.
    unsigned short*     blocks;        // Light grid block indices.
    float               reverb[NUM_REVERB_DATA];
    msector_t           buildData;
} Sector;

// Sidedef sections.
typedef enum sidedefsection_e {
    SS_MIDDLE,
    SS_TOP,
    SS_BOTTOM
} sidedefsection_t;

// Helper macros for accessing sidedef top/middle/bottom section data elements.
#define SW_surface(n)           sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfaceinflags(n)    SW_surface(n).inFlags
#define SW_surfacematerial(n)   SW_surface(n).material
#define SW_surfacetangent(n)    SW_surface(n).tangent
#define SW_surfacebitangent(n)  SW_surface(n).bitangent
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfaceoffset(n)     SW_surface(n).offset
#define SW_surfacevisoffset(n)  SW_surface(n).visOffset
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfaceblendmode(n)  SW_surface(n).blendMode

#define SW_middlesurface        SW_surface(SS_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SS_MIDDLE)
#define SW_middleinflags        SW_surfaceinflags(SS_MIDDLE)
#define SW_middlematerial       SW_surfacematerial(SS_MIDDLE)
#define SW_middletangent        SW_surfacetangent(SS_MIDDLE)
#define SW_middlebitangent      SW_surfacebitangent(SS_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SS_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SS_MIDDLE)
#define SW_middleoffset         SW_surfaceoffset(SS_MIDDLE)
#define SW_middlevisoffset      SW_surfacevisoffset(SS_MIDDLE)
#define SW_middlergba           SW_surfacergba(SS_MIDDLE)
#define SW_middleblendmode      SW_surfaceblendmode(SS_MIDDLE)

#define SW_topsurface           SW_surface(SS_TOP)
#define SW_topflags             SW_surfaceflags(SS_TOP)
#define SW_topinflags           SW_surfaceinflags(SS_TOP)
#define SW_topmaterial          SW_surfacematerial(SS_TOP)
#define SW_toptangent           SW_surfacetangent(SS_TOP)
#define SW_topbitangent         SW_surfacebitangent(SS_TOP)
#define SW_topnormal            SW_surfacenormal(SS_TOP)
#define SW_toptexmove           SW_surfacetexmove(SS_TOP)
#define SW_topoffset            SW_surfaceoffset(SS_TOP)
#define SW_topvisoffset         SW_surfacevisoffset(SS_TOP)
#define SW_toprgba              SW_surfacergba(SS_TOP)

#define SW_bottomsurface        SW_surface(SS_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SS_BOTTOM)
#define SW_bottominflags        SW_surfaceinflags(SS_BOTTOM)
#define SW_bottommaterial       SW_surfacematerial(SS_BOTTOM)
#define SW_bottomtangent        SW_surfacetangent(SS_BOTTOM)
#define SW_bottombitangent      SW_surfacebitangent(SS_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SS_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SS_BOTTOM)
#define SW_bottomoffset         SW_surfaceoffset(SS_BOTTOM)
#define SW_bottomvisoffset      SW_surfacevisoffset(SS_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SS_BOTTOM)

#define FRONT                   0
#define BACK                    1

typedef struct msidedef_s {
    // Sidedef index. Always valid after loading & pruning.
    int         index;
    int         refCount;
} msidedef_t;

typedef struct sidedef_s {
    runtime_mapdata_header_t header;
    Surface             sections[3];
    unsigned int        hedgeCount;
    struct hedge_s**    hedges;          // [hedgeCount] size, hedges arranged left>right
    struct linedef_s*   line;
    struct sector_s*    sector;
    short               flags;
    origin_t            origin;
    msidedef_t          buildData;
    int                 fakeRadioUpdateCount; // frame number of last update
    shadowcorner_t      topCorners[2];
    shadowcorner_t      bottomCorners[2];
    shadowcorner_t      sideCorners[2];
    edgespan_t          spans[2];      // [left, right]
} SideDef;

// Helper macros for accessing linedef data elements.
#define L_v(n)                  v[(n)? 1:0]
#define L_vpos(n)               v[(n)? 1:0]->V_pos

#define L_v1                    L_v(0)
#define L_v1pos                 L_v(0)->V_pos

#define L_v2                    L_v(1)
#define L_v2pos                 L_v(1)->V_pos

#define L_vo(n)                 vo[(n)? 1:0]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_side(n)               sideDefs[(n)? 1:0]
#define L_frontside             L_side(FRONT)
#define L_backside              L_side(BACK)
#define L_sector(n)             sideDefs[(n)? 1:0]->sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Is this line self-referencing (front sec == back sec)?
#define LINE_SELFREF(l)			((l)->L_frontside && (l)->L_backside && \
								 (l)->L_frontsector == (l)->L_backsector)

// Internal flags:
#define LF_POLYOBJ				0x1 // Line is part of a polyobject.

#define MLF_TWOSIDED            0x1 // Line is marked two-sided.
#define MLF_ZEROLENGTH          0x2 // Zero length (line should be totally ignored).
#define MLF_SELFREF             0x4 // Sector is the same on both sides.
#define MLF_POLYOBJ             0x8 // Line is part of a polyobj.

typedef struct mlinedef_s {
    // Linedef index. Always valid after loading & pruning of zero
    // length lines has occurred.
    int         index;
    int         mlFlags; // MLF_* flags.

    // One-sided linedef used for a special effect (windows).
    // The value refers to the opposite sector on the back side.
    struct sector_s *windowEffect;

    // Normally NULL, except when this linedef directly overlaps an earlier
    // one (a rarely-used trick to create higher mid-masked textures).
    // No hedges should be created for these overlapping linedefs.
    struct linedef_s *overlap;
} mlinedef_t;

typedef struct linedef_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v[2];
    struct lineowner_s* vo[2];         // Links to vertex line owner nodes [left, right]
    struct sidedef_s*   sideDefs[2];
    int                 flags;         // Public DDLF_* flags.
    byte                inFlags;       // Internal LF_* flags
    slopetype_t         slopeType;
    int                 validCount;
    binangle_t          angle;         // Calculated from front side's normal
    float               dX;
    float               dY;
    float               length;        // Accurate length
    AABoxf              aaBox;
    boolean             mapped[DDMAXPLAYERS]; // Whether the line has been mapped by each player yet.
    mlinedef_t          buildData;
    unsigned short      shadowVisFrame[2]; // Framecount of last time shadows were drawn for this line, for each side [right, left].
} LineDef;

#define RIGHT                   0
#define LEFT                    1

/**
 * An infinite line of the form point + direction vectors.
 */
typedef struct partition_s {
    float x, y;
    float dX, dY;
} partition_t;

typedef struct bspnode_s {
    runtime_mapdata_header_t header;
    partition_t         partition;
    float               bBox[2][4];    // Bounding box for each child.
    unsigned int        children[2];   // If NF_LEAF it's a BspLeaf.
} BspNode;

#endif
