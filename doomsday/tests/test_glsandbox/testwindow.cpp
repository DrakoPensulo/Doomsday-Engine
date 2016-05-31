/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "testwindow.h"

#include <QMessageBox>
#include <QPainter>
#include <QGLFormat>

#include <de/ImageBank>
#include <de/GLState>
#include <de/Drawable>
#include <de/GLBuffer>
#include <de/GLShader>
#include <de/GLTexture>
#include <de/GLTarget>
#include <de/AtlasTexture>
#include <de/ModelDrawable>
#include <de/GuiApp>
#include <de/Clock>

using namespace de;

DENG2_PIMPL(TestWindow),
DENG2_OBSERVES(Canvas, GLInit),
DENG2_OBSERVES(Canvas, GLResize),
DENG2_OBSERVES(Clock, TimeChange),
DENG2_OBSERVES(Bank, Load)
{
    QToolBar *modelChoice;

    enum Mode
    {
        TestRenderToTexture,
        TestDynamicAtlas,
        TestModel
    };

    Mode mode;
    ImageBank imageBank;
    Drawable ob;
    Drawable atlasOb;
    Matrix4f modelMatrix;
    Matrix4f projMatrix;
    GLUniform uMvpMatrix;
    GLUniform uColor;
    GLUniform uTime;
    GLUniform uTex;
    GLTexture frameTex;
    GLTexture testpic;
    ModelDrawable model;
    ModelDrawable::Animator modelAnim;
    QScopedPointer<AtlasTexture> modelAtlas;
    GLUniform uModelTex;
    GLProgram modelProgram;
    QScopedPointer<AtlasTexture> atlas;
    QScopedPointer<GLTarget> frameTarget;
    Time startedAt;
    Time lastAtlasAdditionAt;
    bool eraseAtlas;

    typedef GLBufferT<Vertex3TexRgba> VertexBuf;
    typedef GLBufferT<Vertex2Tex> Vertex2Buf;

    Instance(Public *i)
        : Base(i),
          mode      (TestRenderToTexture),
          //imageBank (0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4),
          uTime     ("uTime",      GLUniform::Float),
          uTex      ("uTex",       GLUniform::Sampler2D),
          modelAnim (model),
          uModelTex ("uTex",       GLUniform::Sampler2D),
          atlas     (AtlasTexture::newWithRowAllocator(Atlas::AllowDefragment |
                                                          Atlas::BackingStore |
                                                          Atlas::WrapBordersInBackingStore))
    {
        // Use this as the main window.
        setMain(i);

        self.canvas().audienceForGLInit() += this;
        self.canvas().audienceForGLResize() += this;
        Clock::get().audienceForTimeChange() += this;

        uColor = Vector4f(.5f, .75f, .5f, 1);
        atlas->setTotalSize(Vector2ui(256, 256));
        atlas->setBorderSize(2);
        atlas->setMagFilter(gl::Nearest);

        imageBank.add("rtt.cube", "/packs/net.dengine.test.glsandbox/testpic.png");
        //imageBank.loadAll();
        imageBank.audienceForLoad() += this;

        //model.load(App::rootFolder().locate<File>("/data/models/marine.md2")); //boblampclean.md5mesh"));

        modelAtlas.reset(AtlasTexture::newWithKdTreeAllocator(Atlas::DefaultFlags, Atlas::Size(2048, 2048)));
        model.setAtlas(*modelAtlas);
        uModelTex = *modelAtlas;
    }

    ~Instance()
    {
        model.glDeinit();
    }

    void canvasGLInit(Canvas &cv)
    {
        try
        {
            LOG_DEBUG("GLInit");
            glInit(cv);
        }
        catch (Error const &er)
        {
            qWarning() << er.asText();

            QMessageBox::critical(thisPublic, "GL Init Error", er.asText());
            exit(1);
        }
    }

    void glInit(Canvas &cv)
    {
        // Set up the default state.
        GLState &st = GLState::current();
        st.setBlend(true);
        st.setBlendFunc(gl::SrcAlpha, gl::OneMinusSrcAlpha);
        //st.setCull(gl::Back);
        st.setDepthTest(true);

        // Textures.
        testpic.setAutoGenMips(true);
        imageBank.load("rtt.cube");
        //testpic.setImage(imageBank.image("rtt/cube"));
        //testpic.setImage(QImage(":/images/testpic.png"));
        testpic.setWrapT(gl::RepeatMirrored);
        testpic.setMinFilter(gl::Linear, gl::MipLinear);
        uTex = testpic;

        // Prepare the custom target.
        frameTex.setUndefinedImage(Vector2ui(512, 256), Image::RGBA_8888);
        frameTarget.reset(new GLTarget(frameTex));

        // 3D cube.
        VertexBuf *buf = new VertexBuf;
        ob.addBuffer(buf);

        VertexBuf::Type verts[8] = {
            { Vector3f(-1, -1, -1), Vector2f(0, 0), Vector4f(1, 1, 1, 1) },
            { Vector3f( 1, -1, -1), Vector2f(1, 0), Vector4f(1, 1, 0, 1) },
            { Vector3f( 1,  1, -1), Vector2f(1, 1), Vector4f(1, 0, 0, 1) },
            { Vector3f(-1,  1, -1), Vector2f(0, 1), Vector4f(0, 0, 1, 1) },
            { Vector3f(-1, -1,  1), Vector2f(1, 1), Vector4f(1, 1, 1, 1) },
            { Vector3f( 1, -1,  1), Vector2f(0, 1), Vector4f(1, 1, 0, 1) },
            { Vector3f( 1,  1,  1), Vector2f(0, 0), Vector4f(1, 0, 0, 1) },
            { Vector3f(-1,  1,  1), Vector2f(1, 0), Vector4f(0, 0, 1, 1) }
        };

        buf->setVertices(verts, 8, gl::Static);

        GLBuffer::Indices idx;
        idx << 0 << 4 << 3 << 7 << 2 << 6 << 1 << 5 << 0 << 4
            << 4 << 0
            << 0 << 3 << 1 << 2
            << 2 << 7
            << 7 << 4 << 6 << 5;
        buf->setIndices(gl::TriangleStrip, idx, gl::Static);

        ob.program().build(
                    ByteRefArray::fromCStr(
                        "uniform highp mat4 uMvpMatrix;\n"
                        "uniform highp vec4 uColor;\n"
                        "uniform highp float uTime;\n"

                        "attribute highp vec4 aVertex;\n"
                        "attribute highp vec2 aUV;\n"
                        "attribute highp vec4 aColor;\n"

                        "varying highp vec2 vUV;\n"
                        "varying highp vec4 vColor;\n"

                        "void main(void) {\n"
                        "  gl_Position = uMvpMatrix * aVertex;\n"
                        "  vUV = aUV + vec2(uTime/10.0, 0.0);\n"
                        "  vColor = aColor + vec4(sin(uTime), cos(uTime), "
                            "sin(uTime), cos(uTime)*0.5) * uColor;\n"
                        "}\n"),
                    ByteRefArray::fromCStr(
                        "uniform sampler2D uTex;\n"

                        "varying highp vec2 vUV;\n"
                        "varying highp vec4 vColor;\n"

                        "void main(void) {\n"
                        "  highp vec4 color = texture2D(uTex, vUV);\n"
                        "  if (color.a < 0.05) discard;\n"
                        "  gl_FragColor = color * vColor;\n"
                        "}"))
                << uMvpMatrix
                << uColor << uTime
                << uTex;

        // Require testpic to be ready before rendering the cube.
        ob += testpic;

        // The atlas objects.
        Vertex2Buf *buf2 = new Vertex2Buf;
        Vertex2Buf::Type verts2[4] = {
            { Vector2f(0, 0),     Vector2f(0, 0) },
            { Vector2f(100, 0),   Vector2f(1, 0) },
            { Vector2f(100, 100), Vector2f(1, 1) },
            { Vector2f(0, 100),   Vector2f(0, 1) }
        };
        buf2->setVertices(gl::TriangleFan, verts2, 4, gl::Static);
        atlasOb.addBuffer(buf2);

        atlasOb.program().build(
                    ByteRefArray::fromCStr(
                        "uniform highp mat4 uMvpMatrix;\n"
                        "attribute highp vec4 aVertex;\n"
                        "attribute highp vec2 aUV;\n"
                        "varying highp vec2 vUV;\n"
                        "void main(void) {\n"
                        "  gl_Position = uMvpMatrix * aVertex;\n"
                        "  vUV = aUV;\n"
                        "}\n"),
                    ByteRefArray::fromCStr(
                        "uniform sampler2D uTex;\n"
                        "varying highp vec2 vUV;\n"
                        "void main(void) {\n"
                        "  gl_FragColor = texture2D(uTex, vUV);\n"
                        "}\n"))
                << uMvpMatrix // note: uniforms shared between programs
                << uTex;

        cv.renderTarget().setClearColor(Vector4f(.2f, .2f, .2f, 0));

        modelProgram.build(
                    ByteRefArray::fromCStr(
                        "uniform highp mat4 uMvpMatrix;\n"
                        "uniform highp vec4 uColor;\n"
                        "uniform highp mat4 uBoneMatrices[64];\n"

                        "attribute highp vec4 aVertex;\n"
                        "attribute highp vec3 aNormal;\n"
                        "attribute highp vec2 aUV;\n"
                        "attribute highp vec4 aBounds;\n"
                        "attribute highp vec4 aColor;\n"
                        "attribute highp vec4 aBoneIDs;\n"
                        "attribute highp vec4 aBoneWeights;\n"

                        "varying highp vec2 vUV;\n"
                        "varying highp vec4 vColor;\n"
                        "varying highp vec3 vNormal;\n"

                        "void main(void) {\n"
                        "  highp mat4 bone =\n"
                        "    uBoneMatrices[int(aBoneIDs.x + 0.5)] * aBoneWeights.x + \n"
                        "    uBoneMatrices[int(aBoneIDs.y + 0.5)] * aBoneWeights.y + \n"
                        "    uBoneMatrices[int(aBoneIDs.z + 0.5)] * aBoneWeights.z + \n"
                        "    uBoneMatrices[int(aBoneIDs.w + 0.5)] * aBoneWeights.w;\n"
                        "  highp vec4 modelPos = bone * aVertex;\n"
                        "  gl_Position = uMvpMatrix * modelPos;\n"
                        "  vUV = aBounds.xy + aUV * aBounds.zw;\n"
                        "  vColor = aColor;\n"
                        "  vNormal = (bone * vec4(aNormal, 0.0)).xyz;\n"
                        "}\n"),
                    ByteRefArray::fromCStr(
                        "uniform sampler2D uTex;\n"
                        "varying highp vec2 vUV;\n"
                        "varying highp vec3 vNormal;\n"
                        "void main(void) {\n"
                        "  gl_FragColor = texture2D(uTex, vUV) * "
                            "vec4(vec3((vNormal.x + 1.0) / 2.0), 1.0);"
                        "}\n"))
                << uMvpMatrix
                << uModelTex;
        model.setProgram(&modelProgram);
    }

    void bankLoaded(DotPath const &path)
    {
        LOG_RES_NOTE("Bank item \"%s\" loaded") << path;
        if (path == "rtt.cube")
        {
            DENG2_ASSERT_IN_MAIN_THREAD();

            self.canvas().makeCurrent();
            testpic.setImage(imageBank.image(path));
            //self.canvas().doneCurrent();

            imageBank.unload(path);
        }
    }

    void canvasGLResized(Canvas &cv)
    {
        LOG_GL_VERBOSE("GLResized: %i x %i") << cv.width() << cv.height();

        GLState &st = GLState::current();
        //st.setViewport(Rectangleui::fromSize(cv.size()));
        st.setViewport(Rectangleui(0, 0, cv.width(), cv.height()));

        updateProjection(cv);
    }

    void updateProjection(Canvas &cv)
    {
        switch (mode)
        {
        case TestRenderToTexture:
            // 3D projection.
            projMatrix = Matrix4f::perspective(40, float(cv.width())/float(cv.height())) *
                         Matrix4f::lookAt(Vector3f(), Vector3f(0, 0, -5), Vector3f(0, -1, 0));
            break;

        case TestDynamicAtlas:
            // 2D projection.
            uMvpMatrix = projMatrix =
                    Matrix4f::ortho(-cv.width()/2,  cv.width()/2,
                                    -cv.height()/2, cv.height()/2) *
                    Matrix4f::scale(cv.height()/150.f) *
                    Matrix4f::translate(Vector2f(-50, -50));
            break;

        case TestModel:
            // 3D projection.
            projMatrix = Matrix4f::perspective(40, float(cv.width())/float(cv.height())) *
                         Matrix4f::lookAt(Vector3f(), Vector3f(0, -3, 0), Vector3f(0, 0, 1));
            break;
        }
    }

    void setMode(Mode newMode)
    {
        mode = newMode;
        updateProjection(self.canvas());

        modelChoice->hide();

        switch (mode)
        {
        case TestDynamicAtlas:
            lastAtlasAdditionAt = Time();
            uMvpMatrix = projMatrix;
            break;

        case TestModel:
            modelChoice->show();
            break;

        default:
            break;
        }
    }

    void draw(Canvas &)
    {
        switch (mode)
        {
        case TestRenderToTexture:
            // First render the frame to the texture.
            GLState::push()
                .setTarget(*frameTarget)
                .setViewport(Rectangleui::fromSize(frameTex.size()));
            drawRttFrame();
            GLState::pop();

            // Render normally.
            drawRttFrame();
            break;

        case TestDynamicAtlas:
            GLState::push().setBlend(false);
            drawAtlasFrame();
            GLState::pop();
            break;

        case TestModel:
            drawModel();
            break;
        }
    }

    void drawRttFrame()
    {
        GLState::current().target().clear(GLTarget::ColorDepth);

        // The left cube.
        uTex = testpic;
        uMvpMatrix = projMatrix *
                     Matrix4f::translate(Vector3f(-1.5f, 0, 0)) *
                     modelMatrix;
        ob.draw();

        // The right cube.
        uTex = frameTex;
        uMvpMatrix = projMatrix *
                     Matrix4f::translate(Vector3f(1.5f, 0, 0)) *
                     modelMatrix;
        ob.draw();
    }

    void drawAtlasFrame()
    {
        GLState::current().target().clear(GLTarget::ColorDepth);
        uTex = *atlas;
        atlasOb.draw();
    }

    void initModelAnimation()
    {
        modelAnim.clear();
        modelAnim.start(0);
    }

    void drawModel()
    {
        GLState::current().target().clear(GLTarget::ColorDepth);

        uMvpMatrix = projMatrix * modelMatrix;

        if (!modelAnim.isEmpty())
        {
            modelAnim.at(0).time = startedAt.since();
        }

        model.draw(&modelAnim);
    }

    void timeChanged(Clock const &clock)
    {
        if (!startedAt.isValid())
        {
            startedAt = clock.time();
        }
        uTime = startedAt.since();

        switch (mode)
        {
        case TestRenderToTexture:
            modelMatrix = Matrix4f::rotate(std::cos(uTime.toFloat()/2) * 45, Vector3f(1, 0, 0)) *
                          Matrix4f::rotate(std::sin(uTime.toFloat()/3) * 60, Vector3f(0, 1, 0));
            break;

        case TestModel:
            modelMatrix = Matrix4f::translate(Vector3f(0, std::cos(uTime.toFloat()/2.5f), 0)) *
                          Matrix4f::rotate(std::cos(uTime.toFloat()/2) * 45, Vector3f(1, 0, 0)) *
                          Matrix4f::rotate(std::sin(uTime.toFloat()/3) * 60, Vector3f(0, 1, 0)) *
                          Matrix4f::scale(3.f / de::max(model.dimensions().x, model.dimensions().y, model.dimensions().z)) *
                          Matrix4f::translate(-model.midPoint());
            break;

        case TestDynamicAtlas:
            if (lastAtlasAdditionAt.since() > 0.2)
            {
                lastAtlasAdditionAt = Time();
                nextAtlasAlloc();
            }
            break;
        }

        self.canvas().update();
    }

    void nextAtlasAlloc()
    {
        if (eraseAtlas)
        {
            atlas->clear();
            eraseAtlas = false;
            return;
        }

#if 1
        if ((qrand() % 10) <= 5 && !atlas->isEmpty())
        {
            // Randomly remove one of the allocations.
            QList<Id> ids;
            foreach (Id const &id, atlas->allImages()) ids << id;
            Id chosen = ids[qrand() % ids.size()];
            atlas->release(chosen);

            LOG_DEBUG("Removed ") << chosen;
        }
#endif

        // Generate a random image.
        QSize imgSize(10 + qrand() % 40, 10 + 10 * (qrand() % 2));
        QImage img(imgSize, QImage::Format_ARGB32);
        QPainter painter(&img);
        painter.fillRect(img.rect(), QColor(qrand() % 256, qrand() % 256, qrand() % 256));
        painter.setPen(Qt::white);
        painter.drawRect(img.rect().adjusted(0, 0, -1, -1));

        Id id = atlas->alloc(img);
        LOG_DEBUG("Allocated ") << id;
        if (id.isNone())
        {
            lastAtlasAdditionAt = Time() + 5.0;

            // Erase the entire atlas.
            eraseAtlas = true;
        }
    }
};

TestWindow::TestWindow() : d(new Instance(this))
{
    qsrand(Time().asDateTime().toTime_t());

    setWindowTitle("libgui GL Sandbox");
    setMinimumSize(640, 480);

    QToolBar *tools = addToolBar(tr("Tests"));
    tools->addAction("RTT", this, SLOT(testRenderToTexture()));
    tools->addAction("Atlas", this, SLOT(testDynamicAtlas()));
    tools->addAction("Model", this, SLOT(testModel()));

    d->modelChoice = addToolBar(tr("Models"));
    d->modelChoice->addAction("MD2", this, SLOT(loadMD2Model()));
    d->modelChoice->addAction("MD5", this, SLOT(loadMD5Model()));
    //addToolBar(Qt::TopToolBarArea, d->modelChoice);
    //d->modelChoice->hide();
}

void TestWindow::canvasGLDraw(Canvas &canvas)
{
    LIBGUI_ASSERT_GL_OK();

    d->draw(canvas);
    canvas.swapBuffers();

    CanvasWindow::canvasGLDraw(canvas);
}

void TestWindow::testRenderToTexture()
{
    d->setMode(Instance::TestRenderToTexture);
}

void TestWindow::testDynamicAtlas()
{
    d->setMode(Instance::TestDynamicAtlas);
}

void TestWindow::testModel()
{
    d->setMode(Instance::TestModel);
}

void TestWindow::loadMD2Model()
{
    d->model.load(App::rootFolder().locate<File>("/packs/net.dengine.test.glsandbox/models/marine.md2"));
    d->initModelAnimation();
}

void TestWindow::loadMD5Model()
{
    d->model.load(App::rootFolder().locate<File>("/packs/net.dengine.test.glsandbox/models/boblampclean.md5mesh"));
    d->initModelAnimation();
}
