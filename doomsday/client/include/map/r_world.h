/** @file r_world.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * World Setup and Refresh.
 */

#ifndef LIBDENG_REFRESH_WORLD_H
#define LIBDENG_REFRESH_WORLD_H

#include "resource/r_data.h"
#include "map/vertex.h"
#include "map/sector.h"
#include "map/sidedef.h"
#include "map/plane.h"
#include "map/sidedef.h"
#include <de/Observers>

/// @todo The MapChange audience belongs in a class.
DENG2_DECLARE_AUDIENCE(MapChange, void currentMapChanged())
DENG2_EXTERN_AUDIENCE(MapChange)

// Used for vertex sector owners, side line owners and reverb BSP leafs.
typedef struct ownernode_s {
    void*           data;
    struct ownernode_s* next;
} ownernode_t;

typedef struct {
    ownernode_t*    head;
    uint            count;
} ownerlist_t;

extern float rendSkyLight; // cvar
extern byte rendSkyLightAuto; // cvar
extern float rendLightWallAngle;
extern byte rendLightWallAngleSmooth;
extern boolean ddMapSetup;
extern boolean firstFrameAfterLoad;

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 // Always draw the sky sphere.

/**
 * Sector light color may be affected by the sky light color.
 */
const_pvec3f_t &R_GetSectorLightColor(Sector const *sector);

float           R_DistAttenuateLightLevel(float distToViewer, float lightLevel);

/**
 * The DOOM lighting model applies a light level delta to everything when
 * e.g. the player shoots.
 *
 * @return  Calculated delta.
 */
float R_ExtraLightDelta(void);

/**
 * @return  @c > 0 if @a lightlevel passes the min max limit condition.
 */
float R_CheckSectorLight(float lightlevel, float min, float max);

boolean R_SectorContainsSkySurfaces(const Sector* sec);

void R_UpdatePlanes(void);
void R_ClearSectorFlags(void);
void R_MapInitSurfaceLists(void);

/**
 * Returns pointers to the line's vertices in such a fashion that @c verts[0]
 * is the leftmost vertex and @c verts[1] is the rightmost vertex, when the
 * @a line lies at the edge of @a sector.
 */
void R_OrderVertices(LineDef *line, Sector const *sector, Vertex *verts[2]);

/**
 * Determine the map space Z coordinates of a wall section.
 *
 * @param section       Identifier of the section to determine coordinates for.
 * @param lineFlags     @ref ldefFlags.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c NULL
 * @param frontDef      Definition for the front side. Can be @c NULL
 * @param backDef       Definition for the back side. Can be @c NULL
 *
 * Return values:
 * @param low           Z map space coordinate at the bottom of the wall section.
 * @param hi            Z map space coordinate at the top of the wall section.
 * @param matOffset     Surface space material coordinate offset. Can be @c NULL
 *
 * @return  @c true iff the determined wall section height is @c >0
 */
boolean R_FindBottomTop2(SideDefSection section, int lineFlags,
    Sector const *frontSec, Sector const *backSec, SideDef const *frontDef, SideDef const *backDef,
    coord_t *low, coord_t *hi, pvec2f_t matOffset);
boolean R_FindBottomTop(SideDefSection section, int lineFlags,
    Sector const *frontSec, Sector const *backSec, SideDef const *frontDef, SideDef const *backDef,
    coord_t *low, coord_t *hi) /* matOffset = 0 */;

/**
 * Find the "sharp" Z coordinate range of the opening between sectors @a frontSec
 * and @a backSec. The open range is defined as the gap between foor and ceiling on
 * the front side clipped by the floor and ceiling planes on the back side (if present).
 *
 * @param frontSec  Sector on the front side.
 * @param backSec   Sector on the back side. Can be @c NULL.
 * @param bottom    Bottom Z height is written here. Can be @c NULL.
 * @param top       Top Z height is written here. Can be @c NULL.
 *
 * @return Height of the open range.
 */
coord_t R_OpenRange(Sector const* frontSec, Sector const* backSec, coord_t* retBottom, coord_t* retTop);

/// Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
/// height coordinates rather than the "sharp" coordinates.
coord_t R_VisOpenRange(Sector const* frontSec, Sector const* backSec, coord_t* retBottom, coord_t* retTop);

#ifdef __CLIENT__
/**
 * @param lineFlags     @ref ldefFlags.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c NULL
 * @param frontDef      Definition for the front side. Can be @c NULL
 * @param backDef       Definition for the back side. Can be @c NULL
 * @param ignoreOpacity @c true= material opacity should be ignored.
 *
 * @return  @c true iff SideDef @a frontDef has a "middle" Material which completely
 *     covers the open range defined by sectors @a frontSec and @a backSec.
 */
boolean R_MiddleMaterialCoversOpening(int lineFlags, Sector const *frontSec,
    Sector const *backSec, SideDef const *frontDef, SideDef const *backDef,
    boolean ignoreOpacity);

/**
 * Same as @ref R_MiddleMaterialCoversOpening except all arguments are derived from
 * the specified linedef @a line.
 *
 * @note Anything calling this is likely working at the wrong level (should work with
 *       hedges instead).
 */
boolean R_MiddleMaterialCoversLineOpening(LineDef const *line, int side, boolean ignoreOpacity);
#endif // __CLIENT__

Plane*          R_NewPlaneForSector(Sector* sec);
void            R_DestroyPlaneOfSector(uint id, Sector* sec);

void            R_UpdateTrackedPlanes(void);
void            R_InterpolateTrackedPlanes(boolean resetNextViewer);

void            R_AddTrackedPlane(PlaneSet* plist, Plane* pln);
boolean         R_RemoveTrackedPlane(PlaneSet* plist, Plane* pln);

void            R_UpdateSurfaceScroll(void);
void            R_InterpolateSurfaceScroll(boolean resetNextViewer);

boolean R_UpdateSector(Sector *sec, boolean forceUpdate);
boolean R_UpdateLinedef(LineDef *line, boolean forceUpdate);
boolean R_UpdateSidedef(SideDef *sideDef, boolean forceUpdate);
boolean R_UpdatePlane(Plane *pln, boolean forceUpdate);
boolean R_UpdateSurface(Surface *suf, boolean forceUpdate);

/**
 * To be called in response to a Material property changing which may
 * require updating any map surfaces which are presently using it.
 */
void R_UpdateMapSurfacesOnMaterialChange(Material* material);

/// @return  @c true= @a plane is non-glowing (i.e. not glowing or a sky).
boolean R_IsGlowingPlane(const Plane* plane);

/// @return  Current glow strength for the plane.
float R_GlowStrength(const Plane* pln);

LineOwner* R_GetVtxLineOwner(const Vertex* vtx, const LineDef* line);

#ifdef __CLIENT__
/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
LineDef* R_FindLineNeighbor(const Sector* sector, const LineDef* line,
    const LineOwner* own, boolean antiClockwise, binangle_t* diff);

LineDef* R_FindSolidLineNeighbor(const Sector *sector, const LineDef* line,
    const LineOwner* own, boolean antiClockwise, binangle_t* diff);

/**
 * A line's align neighbor is a line that shares a vertex with 'line' and
 * whos orientation is aligned with it (thus, making it unnecessary to have
 * a shadow between them. In practice, they would be considered a single,
 * long sidedef by the shadow generator).
 */
LineDef* R_FindLineAlignNeighbor(const Sector* sec, const LineDef* line,
    const LineOwner* own, boolean antiClockwise, int alignment);

/**
 * Find a backneighbour for the given line. They are the neighbouring line
 * in the backsector of the imediate line neighbor.
 */
LineDef* R_FindLineBackNeighbor(const Sector* sector, const LineDef* line,
    const LineOwner* own, boolean antiClockwise, binangle_t* diff);
#endif // __CLIENT__

/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @ingroup flags
 */
///@{
#define SKYCAP_LOWER                0x1
#define SKYCAP_UPPER                0x2
///@}

coord_t R_SkyCapZ(BspLeaf* bspLeaf, int skyCap);

#endif /* LIBDENG_REFRESH_WORLD_H */
