/**
 * @file canvaswindow.cpp
 * Canvas window implementation. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include <QGLFormat>
#include <QMoveEvent>
#include <de/Log>

#include "de_platform.h"
#include "con_main.h"
#include "gl_main.h"
#include "m_args.h"
#include "canvaswindow.h"
#include <assert.h>

struct CanvasWindow::Instance
{
    Canvas* canvas;
    Canvas* recreated;
    void (*moveFunc)(CanvasWindow&);
    bool mouseWasTrapped;

    Instance() : canvas(0), moveFunc(0), mouseWasTrapped(false) {}
};

CanvasWindow::CanvasWindow(QWidget *parent)
    : QMainWindow(parent)
{
    d = new Instance;

    // Create the drawing canvas for this window.
    setCentralWidget(d->canvas = new Canvas); // takes ownership

    // All input goes to the canvas.
    d->canvas->setFocus();
}

CanvasWindow::~CanvasWindow()
{
    delete d;
}

void CanvasWindow::initCanvasAfterRecreation(Canvas& canvas)
{
    CanvasWindow* self = dynamic_cast<CanvasWindow*>(canvas.parentWidget());
    assert(self);

    LOG_DEBUG("About to replace Canvas %p with %p")
            << de::dintptr(self->d->canvas) << de::dintptr(self->d->recreated);

    // Take the existing callbacks of the old canvas.
    self->d->recreated->useCallbacksFrom(*self->d->canvas);

    // Switch the central widget. This will delete the old canvas automatically.
    self->setCentralWidget(self->d->recreated);
    self->d->canvas = self->d->recreated;
    self->d->recreated = 0;

    // Set up the basic GL state for the new canvas.
    self->d->canvas->makeCurrent();
    GL_Init2DState();
    self->d->canvas->doneCurrent();
    self->d->canvas->update();

    // Reacquire the focus.
    self->d->canvas->setFocus();
    if(self->d->mouseWasTrapped)
    {
        self->d->canvas->trapMouse();
    }

    LOG_DEBUG("Canvas replaced with %p") << de::dintptr(self->d->canvas);
}

void CanvasWindow::recreateCanvas()
{
    /// @todo Instead of recreating, there is also the option of modifying the
    /// existing QGLContext -- however, changing its format causes it to be
    /// reset. We are doing it this way because we wish to retain the current
    /// GL context's texture objects by sharing them with the new Canvas.

    /// @todo See if reseting the QGLContext actually causes all textures to be
    /// lost. If not, it would be better to just change the format and
    /// reinitialize the context state.

    // We'll re-trap the mouse after the new canvas is ready.
    d->mouseWasTrapped = canvas().isMouseTrapped();
    canvas().trapMouse(false);

    // Update the GL format for subsequently created Canvases.
    setDefaultGLFormat();

    // Create the replacement Canvas. Once it's created and visible, we'll
    // finish the switch-over.
    d->recreated = new Canvas(this, d->canvas);
    d->recreated->setInitFunc(initCanvasAfterRecreation);

    d->recreated->setGeometry(d->canvas->geometry());
    d->recreated->show();

    LOG_DEBUG("Canvas recreated, old one still exists");
}

Canvas& CanvasWindow::canvas()
{
    assert(d->canvas != 0);
    return *d->canvas;
}

bool CanvasWindow::ownsCanvas(Canvas *c) const
{
    if(!c) return false;
    return (d->canvas == c || d->recreated == c);
}

void CanvasWindow::setMoveFunc(void (*func)(CanvasWindow&))
{
    d->moveFunc = func;
}

void CanvasWindow::closeEvent(QCloseEvent* ev)
{
    /// @todo autosave and quit?
    Con_Execute(CMDS_DDAY, "quit", true, false);
    ev->ignore();
}

void CanvasWindow::moveEvent(QMoveEvent *ev)
{
    QMainWindow::moveEvent(ev);

    if(d->moveFunc)
    {
        d->moveFunc(*this);
    }
}

void CanvasWindow::hideEvent(QHideEvent* ev)
{
    QMainWindow::hideEvent(ev);

    LOG_DEBUG("CanvasWindow: hide event (hidden:%b)") << isHidden();
}

void CanvasWindow::setDefaultGLFormat() // static
{
    // Configure the GL settings for all subsequently created canvases.
    QGLFormat fmt;
    fmt.setDepthBufferSize(16);
    fmt.setStencilBufferSize(8);
    fmt.setDoubleBuffer(true);

    if(ArgExists("-novsync") || !Con_GetByte("vid-vsync"))
    {
        fmt.setSwapInterval(0); // vsync off
    }
    else
    {
        fmt.setSwapInterval(1);
    }

    if(ArgExists("-nofsaa") || !Con_GetByte("vid-fsaa"))
    {
        fmt.setSampleBuffers(false);
    }
    else
    {
        fmt.setSampleBuffers(true); // multisampling on (default: highest available)
        //fmt.setSamples(4);
    }

    QGLFormat::setDefaultFormat(fmt);
}
