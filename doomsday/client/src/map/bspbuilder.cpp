/** @file bspbuilder.cpp BSP Builder.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include <map>
#include <utility>

#include <de/Error>
#include <de/Log>
#include <de/Observers>
#include <de/Vector>

#include "BspLeaf"
#include "HEdge"
#include "Sector"
#include "map/bsp/partitioner.h"

#include "map/bspbuilder.h"

using namespace de;
using namespace bsp;

DENG2_PIMPL_NOREF(BspBuilder)
{
    /// World space partitioner.
    Partitioner partitioner;

    Instance(GameMap const &map) : partitioner(map) {}
};

BspBuilder::BspBuilder(GameMap const &map, int splitCostFactor)
    : d(new Instance(map))
{
    d->partitioner.setSplitCostFactor(splitCostFactor);
}

void BspBuilder::setSplitCostFactor(int newFactor)
{
    d->partitioner.setSplitCostFactor(newFactor);
}

/// Maximum number of warnings to output (of each type) about any problems
/// encountered during the build process.
static int const maxWarningsPerType = 10;

/**
 * Observes the progress of the build and records any issues/problems encountered
 * in the process. When asked, compiles a human-readable report intended to assist
 * mod authors in debugging their maps.
 *
 * @todo Consolidate with the missing material reporting done elsewhere -ds
 */
class Reporter : DENG2_OBSERVES(Partitioner, UnclosedSectorFound),
                 DENG2_OBSERVES(Partitioner, OneWayWindowFound),
                 DENG2_OBSERVES(Partitioner, MigrantHEdgeBuilt),
                 DENG2_OBSERVES(Partitioner, PartialBspLeafBuilt)
{
    /// Record "unclosed sectors".
    /// Sector => world point relatively near to the problem area.
    typedef std::map<Sector *,  Vector2d> UnclosedSectorMap;

    /// Record "one-way window lines".
    /// Line => Sector the back side faces.
    typedef std::map<Line *,  Sector *> OneWayWindowMap;

    /// Record "migrant half-edges".
    /// HEdge => Sector the half-edge faces.
    typedef std::map<HEdge *, Sector *> MigrantHEdgeMap;

    /// Record "partial BSP leafs".
    /// BspLeaf => number of gaps in the leaf.
    typedef std::map<BspLeaf *, uint> PartialBspLeafMap;

public:

    static inline int maxWarnings(int issueCount)
    {
#ifdef DENG_DEBUG
        return issueCount; // No limit.
#else
        return de::min(issueCount, maxWarningsPerType);
#endif
    }

    inline int unclosedSectorCount() const { return (int)_unclosedSectors.size(); }
    inline int oneWayWindowCount() const { return (int)_oneWayWindows.size(); }
    inline int migrantHEdgeCount() const { return (int)_migrantHEdges.size(); }
    inline int partialBspLeafCount() const { return (int)_partialBspLeafs.size(); }

    void writeLog()
    {
        if(int numToLog = maxWarnings(unclosedSectorCount()))
        {
            UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                LOG_WARNING("Sector #%d is unclosed near %s.")
                    << it->first->indexInMap() << it->second.asText();
            }

            if(numToLog < unclosedSectorCount())
                LOG_INFO("(%u more like this)") << (unclosedSectorCount() - numToLog);
        }

        if(int numToLog = maxWarnings(oneWayWindowCount()))
        {
            OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                LOG_VERBOSE("Line #%d seems to be a One-Way Window (back faces sector #%d).")
                    << it->first->indexInMap() << it->second->indexInMap();
            }

            if(numToLog < oneWayWindowCount())
                LOG_INFO("(%u more like this)") << (oneWayWindowCount() - numToLog);
        }

        if(int numToLog = maxWarnings(migrantHEdgeCount()))
        {
            MigrantHEdgeMap::const_iterator it = _migrantHEdges.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                HEdge *hedge = it->first;
                Sector *facingSector = it->second;

                if(hedge->hasLineSide())
                    LOG_WARNING("Sector #%d has migrant half-edge facing #%d (line #%d).")
                        << facingSector->indexInMap()
                        << hedge->lineSide().sector().indexInMap()
                        << hedge->line().indexInMap();
                else
                    LOG_WARNING("Sector #%d has migrant \"mini\" half-edge.")
                        << facingSector->indexInMap();
            }

            if(numToLog < migrantHEdgeCount())
                LOG_INFO("(%u more like this)") << (migrantHEdgeCount() - numToLog);
        }

        if(int numToLog = maxWarnings(partialBspLeafCount()))
        {
            PartialBspLeafMap::const_iterator it = _partialBspLeafs.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                LOG_WARNING("Half-edge list for BSP leaf %p has %u gaps (%u hedges).")
                    << de::dintptr(it->first) << it->second << it->first->hedgeCount();
            }

            if(numToLog < partialBspLeafCount())
                LOG_INFO("(%i more like this)") << (partialBspLeafCount() - numToLog);
        }
    }

protected:
    // Observes Partitioner UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        _unclosedSectors.insert(std::make_pair(&sector, nearPoint));
    }

    // Observes Partitioner OneWayWindowFound.
    void oneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        _oneWayWindows.insert(std::make_pair(&line, &backFacingSector));
    }

    // Observes Partitioner MigrantHEdgeBuilt.
    void migrantHEdgeBuilt(HEdge &hedge, Sector &facingSector)
    {
        _migrantHEdges.insert(std::make_pair(&hedge, &facingSector));
    }

    // Observes Partitioner PartialBspLeafBuilt.
    void partialBspLeafBuilt(BspLeaf &leaf, uint gapCount)
    {
        _partialBspLeafs.insert(std::make_pair(&leaf, gapCount));
    }

private:
    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
    MigrantHEdgeMap   _migrantHEdges;
    PartialBspLeafMap _partialBspLeafs;
};

bool BspBuilder::buildBsp()
{
    Reporter reporter;

    d->partitioner.audienceForUnclosedSectorFound += reporter;
    d->partitioner.audienceForOneWayWindowFound   += reporter;
    d->partitioner.audienceForMigrantHEdgeBuilt   += reporter;
    d->partitioner.audienceForPartialBspLeafBuilt += reporter;

    bool builtOk = false;
    try
    {
        d->partitioner.build();
        builtOk = true;
    }
    catch(Error const &er)
    {
        LOG_AS("BspBuilder");
        LOG_WARNING("%s.") << er.asText();
    }

    reporter.writeLog();

    return builtOk;
}

BspTreeNode *BspBuilder::root() const
{
    return d->partitioner.root();
}

uint BspBuilder::numNodes()
{
    return d->partitioner.numNodes();
}

uint BspBuilder::numLeafs()
{
    return d->partitioner.numLeafs();
}

uint BspBuilder::numHEdges()
{
    return d->partitioner.numHEdges();
}

uint BspBuilder::numVertexes()
{
    return d->partitioner.numVertexes();
}

Vertex &BspBuilder::vertex(uint idx)
{
    DENG2_ASSERT(d->partitioner.vertex(idx).type() == DMU_VERTEX);
    return d->partitioner.vertex(idx);
}

void BspBuilder::take(MapElement *mapElement)
{
    d->partitioner.release(mapElement);
}
