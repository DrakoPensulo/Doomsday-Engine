/** @file abstractgame.h  Abstact base class for games.
 *
 * @authors Copyright © 2013-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_ABSTRACTGAME_H
#define LIBDOOMSDAY_ABSTRACTGAME_H

#include "libdoomsday.h"
#include <de/String>

/**
 * Abstract base class for games. @ingroup game
 *
 * Represents a specific playable game that runs on top of Doomsday. There can
 * be only one game loaded at a time. Examples of games are "Doom II" and
 * "Ultimate Doom".
 *
 * The 'load' command can be used to load a game based on its identifier:
 * <pre>load doom2</pre>
 *
 * @todo Merge this into the Game class. -jk
 */
class LIBDOOMSDAY_PUBLIC AbstractGame
{
public:
    AbstractGame(de::String const &gameId);
    virtual ~AbstractGame();

    /**
     * Sets the game that this game is a variant of. For instance, "Final Doom:
     * Plutonia Experiment" (doom2-plut) is a variant of "Doom II" (doom2).
     *
     * The source game can be used as a fallback for resources, configurations,
     * and other data.
     *
     * @param gameId  Identifier of a game.
     */
    void setVariantOf(de::String const &gameId);

    bool isNull() const;
    de::String id() const;
    de::String variantOf() const;

    virtual de::String title() const = 0;

    DENG2_AS_IS_METHODS()

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOMSDAY_ABSTRACTGAME_H