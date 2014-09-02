#ifndef _KMS_H_
#define _KMS_H_
#include <EGL/egl.h>
#include <stdlib.h>

class GenericBufferObject;
class GenericBufferSurface
{
public: 
    virtual GenericBufferObject* lockBufferObject() = 0;
    virtual void unlockBufferObject(GenericBufferObject*) = 0;
    virtual void* getNativeSurface() const = 0;
};

class GenericBufferManager
{
public:
    virtual ~GenericBufferManager();
    virtual GenericBufferSurface* createSurface(
            int width,
            int height,
            int format,
            int flags) = 0;
    virtual void destroySurface(GenericBufferSurface* surface) = 0;
    virtual void* getNativeDevice() const = 0;
};

class DirectRenderingManagerImpl;
class DirectRenderingManager
{
public:
    DirectRenderingManager(const char* dev);
    ~DirectRenderingManager();
    bool    init();
    unsigned long   getDisplayWidth() const;
    unsigned long   getDisplayHeight() const;
    GenericBufferManager* createBufferManager();

    GenericBufferObject* getBufferObject();
    bool    useBufferObject(GenericBufferObject* bo);
    bool    applyMode();
    void    destroyBufferManager(GenericBufferManager* gbm);
    bool    flip();
    void    deinit();

private:
    DirectRenderingManagerImpl* _impl;
};


class KMS
{
public:
    KMS(const char* dev);
    ~KMS();
public:
    bool init();
    void deinit();
    bool applyMode();
    bool flip();
    EGLNativeDisplayType    getNativeDisplay();
    EGLNativeWindowType     getNativeWindow(); 
    unsigned long getDisplayWidth();
    unsigned long getDisplayHeight();
private:
    DirectRenderingManager* _drm;
    GenericBufferManager*   _gbm;
    GenericBufferSurface*   _surface;
    char*                   _devName;
};

#endif // _KMS_H_

