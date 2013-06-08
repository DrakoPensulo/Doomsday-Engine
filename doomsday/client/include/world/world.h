/** @file world/world.h World.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_WORLD_H
#define DENG_WORLD_H

#include <de/libdeng1.h>

#include <de/Error>
#include <de/Observers>

#include "uri.hh"

#include "world/map.h"

namespace de {

class World
{
public:
    /// No map is currently loaded. @ingroup errors
    DENG2_ERROR(MapError);

    /**
     * Audience that will be notified when the "current" map changes.
     */
    DENG2_DEFINE_AUDIENCE(MapChange, void currentMapChanged())

public:
    /**
     * Constructs a world.
     */
    World();

    bool hasMap() const;

    Map &map() const;

    bool loadMap(Uri const &uri);

    void setupMap(int mode);

    /// @todo Refactor away.
    void clearMap();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

DENG_EXTERN_C boolean ddMapSetup;

#endif // DENG_WORLD_H
