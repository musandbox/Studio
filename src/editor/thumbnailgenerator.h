#ifndef THUMBNAILGENERATOR_H
#define THUMBNAILGENERATOR_H

#include "../irisgl/src/irisglfwd.h"
#include <QThread>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_2_Core>
#include <QMutex>
#include <QSemaphore>
#include <QImage>

enum class ThumbnailRequestType
{
    Material,
    Mesh
};

struct ThumbnailRequest
{
public:
    ThumbnailRequestType type;
    QString path;
    QString id;
};

struct ThumbnailResult
{
    ThumbnailRequestType type;
    QString path;
    QString id;
    QImage thumbnail;
};

class RenderThread : public QThread
{
    Q_OBJECT
public:
    QOffscreenSurface *surface;
    QOpenGLContext *context;

    iris::Texture2DPtr tex;
    iris::RenderTargetPtr renderTarget;

    iris::ForwardRendererPtr renderer;
    iris::ScenePtr scene;
    iris::SceneNodePtr sceneNode;

    iris::CameraNodePtr cam;
    iris::CustomMaterialPtr material;

    QMutex requestMutex;
    QList<ThumbnailRequest> requests;
    QSemaphore requestsAvailable;

    bool shutdown;

    void requestThumbnail(const ThumbnailRequest& request);

    void run() override;

    void initScene();
    void prepareScene(const ThumbnailRequest& request);
    void cleanupScene();

signals:
    void thumbnailComplete(ThumbnailResult* result);

private:
    //ThumbnailRequest& getRequest();
    float getBoundingRadius(iris::SceneNodePtr node);
    void getBoundingSpheres(iris::SceneNodePtr node, QList<iris::BoundingSphere>& spheres);

};

// http://doc.qt.io/qt-5/qtquick-scenegraph-textureinthread-threadrenderer-cpp.html
class ThumbnailGenerator
{
    static ThumbnailGenerator* instance;
    ThumbnailGenerator();
public:
    RenderThread* renderThread;

    static ThumbnailGenerator* getSingleton();

    void requestThumbnail(ThumbnailRequestType type, QString path, QString id = "");

    // must be called to properly shutdown ui components
    void shutdown();
};

#endif // THUMBNAILGENERATOR_H