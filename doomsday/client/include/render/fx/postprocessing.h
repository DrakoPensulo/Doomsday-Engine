/** @file postprocessing.h  World frame post processing.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_FX_POSTPROCESSING_H
#define DENG_CLIENT_FX_POSTPROCESSING_H

#include <de/libdeng2.h>

namespace fx {

/**
 * Post-processing of rendered camera lens frames. Maintains an offscreen
 * render target and provides a way to draw it back to the regular target
 * with shader effects applied.
 */
class PostProcessing
{
public:
    PostProcessing();

    void glInit();
    void glDeinit();

    void begin();
    void end();
    void drawResult();

private:
    DENG2_PRIVATE(d)
};

} // namespace fx

#endif // DENG_CLIENT_FX_POSTPROCESSING_H
