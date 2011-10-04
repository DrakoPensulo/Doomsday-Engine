/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/c_wrapper.h"
#include "de/LegacyCore"

LegacyCore* LegacyCore_New(int* argc, char** argv)
{
    return reinterpret_cast<LegacyCore*>(new de::LegacyCore(*argc, argv));
}

int LegacyCore_RunEventLoop(LegacyCore* lc, int (*loopFunc)(void))
{
    DENG2_SELF(LegacyCore, lc);
    return self->runEventLoop(loopFunc);
}

void LegacyCore_Stop(LegacyCore *lc, int exitCode)
{
    DENG2_SELF(LegacyCore, lc);
    self->stop(exitCode);
}

void LegacyCore_Delete(LegacyCore* lc)
{
    if(lc)
    {
        // It gets stopped automatically.
        delete reinterpret_cast<de::LegacyCore*>(lc);
    }
}
