/** @file rend_decor.cpp Surface Decorations.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <de/memoryzone.h>
#include <de/Vector>

#include "de_platform.h"
#include "de_console.h"
#include "de_render.h"

#include "dd_main.h" // App_World(), remove me
#include "def_main.h"

#include "world/map.h"
#include "BspLeaf"

#include "render/vissprite.h"
#include "WallEdge"

#include "render/rend_decor.h"

using namespace de;

/// Quite a bit of decorations, there!
#define MAX_DECOR_LIGHTS        (16384)

/// No decorations are visible beyond this.
#define MAX_DECOR_DISTANCE      (2048)

/// @todo This abstraction is now unnecessary -ds
struct Decoration
{
    Vector3d origin;
    coord_t maxDistance;
    Surface const *surface;
    BspLeaf *bspLeaf;
    int lumIdx; // Index of linked lumobj, or Lumobj::NoIndex.
    float fadeMul;
    MaterialSnapshot::Decoration const *decor;
    Decoration *next;
};

byte useLightDecorations     = true;
float decorLightBrightFactor = 1;
float decorLightFadeAngle    = .1f;

static uint sourceCount;
static Decoration *sourceFirst;
static Decoration *sourceCursor;

static void plotSourcesForWallSection(LineSide &side, int section);
static void plotSourcesForPlane(Plane &pln);

void Rend_DecorRegister()
{
    C_VAR_BYTE ("rend-light-decor",        &useLightDecorations,    0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-angle",  &decorLightFadeAngle,    0, 0, 1);
    C_VAR_FLOAT("rend-light-decor-bright", &decorLightBrightFactor, 0, 0, 10);
}

static void recycleSources()
{
    sourceCount = 0;
    sourceCursor = sourceFirst;
}

void Rend_DecorInitForMap(Map &map)
{
    DENG_UNUSED(map);
    recycleSources();
}

/**
 * Create a new decoration source.
 */
static Decoration *allocDecorSource()
{
    Decoration *src;

    // If the cursor is NULL, new sources must be allocated.
    if(!sourceCursor)
    {
        // Allocate a new entry.
        src = (Decoration *) Z_Calloc(sizeof(Decoration), PU_APPSTATIC, NULL);

        if(!sourceFirst)
        {
            sourceFirst = src;
        }
        else
        {
            src->next = sourceFirst;
            sourceFirst = src;
        }
    }
    else
    {
        // There are old sources to use.
        src = sourceCursor;

        src->fadeMul     = 0;
        src->lumIdx      = Lumobj::NoIndex;
        src->maxDistance = 0;
        src->origin      = Vector3d();
        src->bspLeaf     = 0;
        src->surface     = 0;
        src->decor       = 0;

        // Advance the cursor.
        sourceCursor = sourceCursor->next;
    }

    return src;
}

/**
 * A source is created from the specified surface decoration.
 */
static void newSource(Surface const &suf, Surface::DecorSource const &dec)
{
    // Out of sources?
    if(sourceCount >= MAX_DECOR_LIGHTS) return;

    ++sourceCount;
    Decoration *src = allocDecorSource();

    // Fill in the data for a new surface decoration.
    src->origin      = dec.origin;
    src->maxDistance = MAX_DECOR_DISTANCE;
    src->bspLeaf     = dec.bspLeaf;
    src->surface     = &suf;
    src->fadeMul     = 1;
    src->decor       = dec.decor;
}

static uint generateDecorLights(MaterialSnapshot::Decoration const &decor,
    Vector2i const &patternOffset, Vector2i const &patternSkip, Surface &suf,
    Material &material, Vector3d const &topLeft_, Vector3d const &/*bottomRight*/,
    Vector2d sufDimensions, Vector3d const &delta, int axis,
    Vector2f const &matOffset, Sector *containingSector)
{
    // Skip values must be at least one.
    Vector2i skip = Vector2i(patternSkip.x + 1, patternSkip.y + 1)
                        .max(Vector2i(1, 1));

    Vector2f repeat = material.dimensions() * skip;
    if(repeat == Vector2f(0, 0))
        return 0;

    Vector3d topLeft = topLeft_ + suf.normal() * decor.elevation;

    float s = de::wrap(decor.pos[0] - material.width() * patternOffset.x + matOffset.x,
                       0.f, repeat.x);

    // Plot decorations.
    uint plotted = 0;
    for(; s < sufDimensions.x; s += repeat.x)
    {
        // Determine the topmost point for this row.
        float t = de::wrap(decor.pos[1] - material.height() * patternOffset.y + matOffset.y,
                           0.f, repeat.y);

        for(; t < sufDimensions.y; t += repeat.y)
        {
            float const offS = s / sufDimensions.x;
            float const offT = t / sufDimensions.y;
            Vector3d offset(offS, axis == VZ? offT : offS, axis == VZ? offS : offT);

            Vector3d origin  = topLeft + delta * offset;
            BspLeaf &bspLeaf = suf.map().bspLeafAt(origin);
            if(containingSector)
            {
                // The point must be inside the correct sector.
                if(bspLeaf.sectorPtr() != containingSector
                   || !bspLeaf.polyContains(origin))
                    continue;
            }

            if(Surface::DecorSource *source = suf.newDecoration())
            {
                source->origin  = origin;
                source->bspLeaf = &bspLeaf;
                source->decor   = &decor;

                plotted += 1;
            }
        }
    }

    return plotted;
}

/**
 * Generate decorations for the specified surface.
 */
static void updateSurfaceDecorations(Surface &suf, Vector2f const &offset,
    Vector3d const &topLeft, Vector3d const &bottomRight, Sector *sec = 0)
{
    Vector3d delta = bottomRight - topLeft;
    if(de::fequal(delta.length(), 0)) return;

    int const axis = suf.normal().maxAxis();

    Vector2d sufDimensions;
    if(axis == 0 || axis == 1)
    {
        sufDimensions.x = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        sufDimensions.y = delta.z;
    }
    else
    {
        sufDimensions.x = std::sqrt(delta.x * delta.x);
        sufDimensions.y = delta.y;
    }

    if(sufDimensions.x < 0) sufDimensions.x = -sufDimensions.x;
    if(sufDimensions.y < 0) sufDimensions.y = -sufDimensions.y;

    // Generate a number of lights.
    MaterialSnapshot const &ms = suf.material().prepare(Rend_MapSurfaceMaterialSpec());

    Material::Decorations const &decorations = suf.material().decorations();
    for(int i = 0; i < decorations.count(); ++i)
    {
        MaterialSnapshot::Decoration const &decor = ms.decoration(i);
        MaterialDecoration const *def = decorations[i];

        generateDecorLights(decor, def->patternOffset(), def->patternSkip(),
                            suf, suf.material(), topLeft, bottomRight, sufDimensions,
                            delta, axis, offset, sec);
    }
}

static void plotSourcesForPlane(Plane &pln)
{
    Surface &surface = pln.surface();
    if(!surface.hasMaterial()) return;

    Sector &sector = pln.sector();
    AABoxd const &sectorAABox = sector.aaBox();

    Vector3d topLeft(sectorAABox.minX,
                     pln.isSectorFloor()? sectorAABox.maxY : sectorAABox.minY,
                     pln.visHeight());

    Vector3d bottomRight(sectorAABox.maxX,
                         pln.isSectorFloor()? sectorAABox.minY : sectorAABox.maxY,
                         pln.visHeight());

    Vector2f offset(-fmod(sectorAABox.minX, 64) - surface.visMaterialOrigin().x,
                    -fmod(sectorAABox.minY, 64) - surface.visMaterialOrigin().y);

    updateSurfaceDecorations(surface, offset, topLeft, bottomRight, &sector);
}

static void plotSourcesForWallSection(LineSide &side, int section)
{
    if(!side.hasSections()) return;
    if(!side.surface(section).hasMaterial()) return;

    // Is the wall section potentially visible?
    HEdge *leftHEdge  = side.leftHEdge();
    HEdge *rightHEdge = side.rightHEdge();

    if(!leftHEdge || !rightHEdge)
        return;

    WallSpec const wallSpec = WallSpec::fromMapSide(side, section);
    WallEdge leftEdge (wallSpec, *leftHEdge, Line::From);
    WallEdge rightEdge(wallSpec, *rightHEdge, Line::To);

    if(!leftEdge.isValid() || !rightEdge.isValid()
       || de::fequal(leftEdge.bottom().z(), rightEdge.top().z()))
        return;

    updateSurfaceDecorations(side.surface(section), -leftEdge.materialOrigin(),
                             leftEdge.top().origin(), rightEdge.bottom().origin());
}

static void plotSourcesForSurface(Surface &suf)
{
    if(suf._decorationData.needsUpdate)
    {
        suf.clearDecorations();

        switch(suf.parent().type())
        {
        case DMU_SIDE: {
            LineSide &side = suf.parent().as<LineSide>();
            plotSourcesForWallSection(side, &side.middle() == &suf? LineSide::Middle :
                                      &side.bottom() == &suf? LineSide::Bottom : LineSide::Top);
            break; }

        case DMU_PLANE:
            plotSourcesForPlane(suf.parent().as<Plane>());
            break;

        default:
            DENG2_ASSERT(0); // Invalid type.
        }

        suf._decorationData.needsUpdate = false;
    }

    if(useLightDecorations)
    {
        Surface::DecorSource const *sources = (Surface::DecorSource const *)suf._decorationData.sources;
        for(int i = 0; i < suf.decorationCount(); ++i)
        {
            newSource(suf, sources[i]);
        }
    }
}

void Rend_DecorBeginFrame()
{
    // This only needs to be done if decorations have been enabled.
    if(!useLightDecorations) return;

    recycleSources();

    /// @todo fixme: do not assume the current map.
    Map &map = App_World().map();
    foreach(Surface *surface, map.decoratedSurfaces())
    {
        plotSourcesForSurface(*surface);
    }
}

/**
 * @return  @c > 0 if @a lightlevel passes the min max limit condition.
 */
static float checkSectorLightLevel(float lightlevel, float min, float max)
{
    // Has a limit been set?
    if(min == max) return 1;
    return de::clamp(0.f, (lightlevel - min) / float(max - min), 1.f);
}

static void addLuminousDecoration(Decoration &src)
{
    MaterialSnapshot::Decoration const *decor = src.decor;

    // Don't add decorations which emit no color.
    if(decor->color == Vector3f(0, 0, 0))
        return;

    // Does it pass the sector light limitation?
    float lightLevel = src.bspLeaf->sector().lightLevel();
    Rend_ApplyLightAdaptation(lightLevel);
    float intensity = checkSectorLightLevel(lightLevel, decor->lightLevels[0], decor->lightLevels[1]);

    if(intensity < .0001f)
        return;

    // Apply the brightness factor (was calculated using sector lightlevel).
    src.fadeMul = intensity * decorLightBrightFactor;
    src.lumIdx = Lumobj::NoIndex;

    if(src.fadeMul <= 0)
        return;

    Lumobj &lum = src.surface->map().addLumobj(
        Lumobj(src.origin, decor->radius, decor->color * src.fadeMul, src.maxDistance));

    // Any lightmaps to configure?
    lum.setLightmap(Lumobj::Side, decor->tex)
       .setLightmap(Lumobj::Down, decor->floorTex)
       .setLightmap(Lumobj::Up,   decor->ceilTex);

    // Remember it's unique index in the decoration for the purpose of drawing
    // halos/lens-flares.
    src.lumIdx = lum.indexInMap();
}

void Rend_DecorAddLuminous()
{
    if(useLightDecorations && sourceFirst != sourceCursor)
    {
        Decoration *src = sourceFirst;
        do
        {
            addLuminousDecoration(*src);
        } while((src = src->next) != sourceCursor);
    }
}

static void projectSource(Decoration const &src)
{
    Surface const &surface = *src.surface;
    MaterialSnapshot::Decoration const *decor = src.decor;

    Lumobj *lum = surface.map().lumobj(src.lumIdx);
    if(!lum) return; // Huh?

    // Is the point in range?
    double distance = R_ViewerLumobjDistance(lum->indexInMap());
    if(distance > lum->maxDistance())
        return;

    // Does it pass the sector light limitation?
    float min = decor->lightLevels[0];
    float max = decor->lightLevels[1];

    float lightLevel = lum->bspLeafAtOrigin().sector().lightLevel();
    Rend_ApplyLightAdaptation(lightLevel);

    float brightness = checkSectorLightLevel(lightLevel, min, max);
    if(!(brightness > 0)) return;

    /*
     * Light decorations become flare-type vissprites.
     */
    vissprite_t *vis = R_NewVisSprite(VSPR_FLARE);

    vis->origin   = lum->origin();
    vis->distance = distance;

    vis->data.flare.isDecoration = true;
    vis->data.flare.tex          = decor->flareTex;
    vis->data.flare.lumIdx       = lum->indexInMap();

    // Color is taken from the associated lumobj.
    V3f_Set(vis->data.flare.color, lum->color().x, lum->color().y, lum->color().z);

    if(decor->haloRadius > 0)
        vis->data.flare.size = de::max(1.f, decor->haloRadius * 60 * (50 + haloSize) / 100.0f);
    else
        vis->data.flare.size = 0;

    // Fade out as distance from viewer increases.
    vis->data.flare.mul = lum->attenuation(distance);

    // Halo brightness drops as the angle gets too big.
    if(decor->elevation < 2 && decorLightFadeAngle > 0) // Close the surface?
    {
        Vector3d const eye(vOrigin[VX], vOrigin[VZ], vOrigin[VY]);
        Vector3d const vecFromOriginToEye = (lum->origin() - eye).normalize();

        float dot = float( -surface.normal().dot(vecFromOriginToEye) );
        if(dot < decorLightFadeAngle / 2)
        {
            vis->data.flare.mul = 0;
        }
        else if(dot < 3 * decorLightFadeAngle)
        {
            vis->data.flare.mul = (dot - decorLightFadeAngle / 2)
                                / (2.5f * decorLightFadeAngle);
        }
    }
}

void Rend_DecorProject()
{
    if(!useLightDecorations) return;

    for(Decoration *src = sourceFirst; src != sourceCursor; src = src->next)
    {
        projectSource(*src);
    }
}
