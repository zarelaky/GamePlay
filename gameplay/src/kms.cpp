#include "kms.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

extern "C" {
#define virtual _virtual_
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm.h>
#include <gbm/gbm.h>
#undef virtual
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


class GenericBufferObjectDestroyListener;
class GenericBufferObject
{
public:
    GenericBufferObject(gbm_bo* bo, gbm_surface* surface)
        : _bo(bo)
        , _surface(surface)
        , _fb_id(0)
        , _listener(NULL)
    {
        gbm_bo_set_user_data(_bo, this, &GenericBufferObject::DestroyCallBackHandler);
        //printf("bufferObject %08x - alloc\n", this);
    }
    ~GenericBufferObject(){
        //printf("bufferObject %08x - release\n", this);
        if (_bo) {
            gbm_surface_release_buffer(_surface, _bo);
            _bo         = NULL;
            _surface    = NULL;
            _fb_id      = 0;
            _listener   = NULL;
        }
    }

    uint32_t getFrameBufferId() const {
        return _fb_id;
    }

    void setFrameBufferId(uint32_t id) {
        _fb_id = id;
    }
    void setDestroyListener(GenericBufferObjectDestroyListener* listener) {
        _listener = listener;
    }
    uint32_t getWidth() {
        if (!_bo) {
            return 0;
        }
        return gbm_bo_get_width(_bo);
    }
    uint32_t getHeight() {
        if (!_bo) {
            return 0;
        }
        return gbm_bo_get_height(_bo);
    }
    uint32_t getStride() {
        if (!_bo) {
            return 0;
        }
        return gbm_bo_get_stride(_bo);
    }
    uint32_t getHandle() {
        if (!_bo) {
            return 0;
        }
        return gbm_bo_get_handle(_bo).u32;
    }
private:
    static void DestroyCallBackHandler(struct gbm_bo *bo, void *data);
private:
    gbm_bo*     _bo;
    gbm_surface*_surface;
	uint32_t    _fb_id;
    GenericBufferObjectDestroyListener* _listener;
};

class GenericBufferObjectDestroyListener {
public:
    virtual void destroying(GenericBufferObject* bo) = 0;
};

void GenericBufferObject::DestroyCallBackHandler(struct gbm_bo *bo, void *data) {
    GenericBufferObject* b = (GenericBufferObject*)data;
    if (b && b->_listener) {
        printf("GenericBufferObject::DestroyCallBackHandler\n");
        b->_listener->destroying((GenericBufferObject*)data);
    }
}


/////////////////////////////////////////////////////////
//GenericBufferSurfaceImpl {{{
class GenericBufferSurfaceImpl : public GenericBufferSurface
{
public:
    GenericBufferSurfaceImpl(gbm_surface* surface):_surface(surface)
    {
    };
    virtual ~GenericBufferSurfaceImpl() {
        if (_surface) {
            gbm_surface_destroy(_surface);
            _surface = NULL;
        }
    }

    virtual GenericBufferObject* lockBufferObject();
    virtual void unlockBufferObject(GenericBufferObject* bo);
    virtual void* getNativeSurface() const;
private:
    gbm_surface* _surface;
};

GenericBufferObject* GenericBufferSurfaceImpl::lockBufferObject() {
    gbm_bo* bo = gbm_surface_lock_front_buffer(_surface);
    if (bo) {
        return new GenericBufferObject(bo, _surface);
    }
    return NULL;
}

void GenericBufferSurfaceImpl::unlockBufferObject(GenericBufferObject* bo)
{
    delete bo;
}

void* GenericBufferSurfaceImpl::getNativeSurface() const {
    return _surface;
}

//}}}
//////////////////////////////////////////////////////////
// GenericBufferManagerImpl {{{

class GenericBufferManagerImpl : public GenericBufferManager
{
public:
    GenericBufferManagerImpl(int drmFD);
    virtual ~GenericBufferManagerImpl();
    virtual GenericBufferSurface* createSurface(
            int width,
            int height,
            int format,
            int flags);
    virtual void destroySurface(GenericBufferSurface* surface);
    virtual bool init() {
        return _dev != NULL;
    }
    virtual void* getNativeDevice() const {
        return _dev;
    }
public:
    gbm_device*     _dev;
};

GenericBufferManagerImpl::GenericBufferManagerImpl(int drmFd)
    : _dev(NULL)
{
    _dev = gbm_create_device(drmFd);
}

GenericBufferManagerImpl::~GenericBufferManagerImpl() {
    if (_dev) {
        gbm_device_destroy(_dev);
        _dev = NULL;
    }
}

GenericBufferSurface* GenericBufferManagerImpl::createSurface(
            int width,
            int height,
            int format,
            int flags) {
        
    gbm_surface* surface = gbm_surface_create(_dev,
			width, height,
			format,
			flags);
    if (surface) {
        return new GenericBufferSurfaceImpl(surface);
    }
    printf("gbm_surface_create failed\n");
    return NULL;
}
void GenericBufferManagerImpl::destroySurface(GenericBufferSurface* surface) {
    delete surface;
}


//}}}
/////////////////////////////////////////////////////////
// DirectRenderingManagerImpl {{{
class DirectRenderingManagerImpl : public GenericBufferObjectDestroyListener
{
    enum { MAX_DISPLAYS = 4 };
public:
    DirectRenderingManagerImpl(const char* dev);
    ~DirectRenderingManagerImpl();
    int getFileDiscriptor();
    bool init();
    unsigned long getDisplayWidth() const;
    unsigned long getDisplayHeight() const;

    bool applyMode();
    bool useBufferObject(GenericBufferObject* bo);
    int flip();
    GenericBufferObject*    getBufferObject();
    GenericBufferManager*   createBufferManager();
    void destroyBufferManager(GenericBufferManager* gbm);

    void deinit();
protected:
    static void PageFlipHandler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data);
    virtual void destroying(GenericBufferObject* bo);
private:
    char*       _devName;
	int         _fd;
	uint32_t    _ndisp;
	uint32_t    _crtc_id[MAX_DISPLAYS];
	uint32_t    _connector_id[MAX_DISPLAYS];
	drmModeRes*         _resource_id;
	drmModeEncoder*     _encoder[MAX_DISPLAYS];
    uint32_t            DISP_ID;
	drmModeModeInfo    *_mode[MAX_DISPLAYS];
	drmModeConnector   *_connectors[MAX_DISPLAYS];
    GenericBufferObject*_bo;
    drmEventContext     _evctx;
    fd_set              _fds;

};
DirectRenderingManagerImpl::DirectRenderingManagerImpl(const char* dev)
    : _devName(strdup(dev))
    , _fd(0)
    , _ndisp(0)
    , _resource_id(0)
    , DISP_ID(0)
{
}
DirectRenderingManagerImpl::~DirectRenderingManagerImpl()
{
}

// DirectRenderingManagerImpl::init {{{
bool    DirectRenderingManagerImpl::init() {
	static const char *modules[] = {
			"omapdrm", "i915", "radeon", "nouveau", "vmwgfx", "exynos"
	};
	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, j;
	uint32_t maxRes, curRes;
    uint32_t connector_id = 0;

	for (i = 0; i < ARRAY_SIZE(modules); i++) {
		printf("trying to load module %s...", modules[i]);
		_fd = drmOpen(modules[i], NULL);
		if (_fd < 0) {
			printf("failed.\n");
		} else {
			printf("success.\n");
			break;
		}
	}

	if (_fd < 0) {
		printf("could not open drm device\n");
		return false;
	}

	resources = drmModeGetResources(_fd);
	if (!resources) {
		printf("drmModeGetResources failed: %s\n", strerror(errno));
		return false;
	}
	_resource_id = resources;

	/* find a connected connector: */
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(_fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* choose the first supported mode */
			_mode[_ndisp]           = &connector->modes[0];
			_connector_id[_ndisp]   = connector->connector_id;

			for (j=0; j<resources->count_encoders; j++) {
				encoder = drmModeGetEncoder(_fd, resources->encoders[j]);
				if (encoder->encoder_id == connector->encoder_id)
					break;

				drmModeFreeEncoder(encoder);
				encoder = NULL;
			}

			if (!encoder) {
				printf("no encoder!\n");
                return false;
			}

			_encoder[_ndisp]    = encoder;
			_crtc_id[_ndisp]    = encoder->crtc_id;
			_connectors[_ndisp] = connector;

			printf("### Display [%d]: CRTC = %d, Connector = %d\n", \
                    _ndisp,
                    _crtc_id[_ndisp],
                    _connector_id[_ndisp]);
			printf("\tMode chosen [%s] : Clock => %d, Vertical refresh => %d, Type => %d\n",
                    _mode[_ndisp]->name,
                    _mode[_ndisp]->clock, 
                    _mode[_ndisp]->vrefresh,
                    _mode[_ndisp]->type);
			printf("\tHorizontal => %d, %d, %d, %d, %d\n",
                   _mode[_ndisp]->hdisplay,
                   _mode[_ndisp]->hsync_start,
                   _mode[_ndisp]->hsync_end, 
                   _mode[_ndisp]->htotal,
                   _mode[_ndisp]->hskew);
			printf("\tVertical => %d, %d, %d, %d, %d\n", 
                   _mode[_ndisp]->vdisplay, 
                   _mode[_ndisp]->vsync_start,
                   _mode[_ndisp]->vsync_end,
                   _mode[_ndisp]->vtotal, 
                   _mode[_ndisp]->vscan);

			/* If a connector_id is specified, use the corresponding display */
			if ((connector_id != -1) && (connector_id == _connector_id[_ndisp]))
				DISP_ID = _ndisp;

			/* If all displays are enabled, choose the connector with maximum
			* resolution as the primary display */
//			if (all_display) {
//				maxRes = _mode[DISP_ID]->vdisplay * _mode[DISP_ID]->hdisplay;
//				curRes = _mode[_ndisp]->vdisplay * _mode[_ndisp]->hdisplay;
//
//				if (curRes > maxRes)
//					DISP_ID = _ndisp;
//			}

			_ndisp++;
		} else {
			drmModeFreeConnector(connector);
		}
	}

	if (_ndisp == 0) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		printf("no connected connector!\n");
        return false;
	}

    _evctx.version              = DRM_EVENT_CONTEXT_VERSION;
    _evctx.page_flip_handler    = &DirectRenderingManagerImpl::PageFlipHandler;

	FD_ZERO(&_fds);
	FD_SET(_fd, &_fds);

    
    return true;
}
//}}}

GenericBufferManager* DirectRenderingManagerImpl::createBufferManager()
{
    GenericBufferManagerImpl* gbm = new GenericBufferManagerImpl(_fd);
    if (!gbm->init()) {
        delete gbm;
        return NULL;
    }
    return gbm;
}

void DirectRenderingManagerImpl::destroyBufferManager(GenericBufferManager* gbm)
{
    delete gbm;
}

void DirectRenderingManagerImpl::PageFlipHandler(
        int fd, 
        unsigned int frame,
		unsigned int sec,
        unsigned int usec, 
        void *data)
{
	int *waiting_for_flip = (int*)data;
	*waiting_for_flip = 0;
}

GenericBufferObject* DirectRenderingManagerImpl::getBufferObject() {
    return _bo;
}
unsigned long DirectRenderingManagerImpl::getDisplayWidth() const
{
    return _mode[DISP_ID]->hdisplay;
}
unsigned long DirectRenderingManagerImpl::getDisplayHeight() const
{
    return _mode[DISP_ID]->vdisplay;
}

bool DirectRenderingManagerImpl::applyMode() {
    assert(_bo);
    if (!_bo || _bo->getFrameBufferId() == 0 || _bo->getFrameBufferId() == -1) {
        return false;
    }
    int ret = drmModeSetCrtc(_fd, _crtc_id[DISP_ID], _bo->getFrameBufferId(),
				0, 0, &_connector_id[DISP_ID], 1, _mode[DISP_ID]);
    if (ret) {
	    printf("display %d failed to set mode: %s\n", DISP_ID, strerror(errno));
	    return false;
    }
    return true;
}

bool DirectRenderingManagerImpl::useBufferObject(GenericBufferObject* bo)
{
    _bo = bo;
    uint32_t fb_id = 0;
	int ret = drmModeAddFB(_fd, 
            bo->getWidth(), 
            bo->getHeight(), 
            24, 32, 
            bo->getStride(), 
            bo->getHandle(), 
            &fb_id);
	if (ret) {
		printf("failed to create fb: %s\n", strerror(errno));
		return false;
	}
    bo->setFrameBufferId(fb_id);
    bo->setDestroyListener(this);
    return true;
}

void DirectRenderingManagerImpl::destroying(GenericBufferObject* bo)
{
    assert(bo);
    printf("bo destroy %08x\n", bo->getFrameBufferId());
   	if (bo && bo->getFrameBufferId()) {
		drmModeRmFB(_fd, bo->getFrameBufferId());
        delete bo;
    }

}

int     DirectRenderingManagerImpl::flip()
{
        int ret(0);
       
        int waiting_for_flip(-1);
		ret = drmModePageFlip(_fd,
                _crtc_id[DISP_ID], 
                _bo->getFrameBufferId(),
				DRM_MODE_PAGE_FLIP_EVENT, 
                &waiting_for_flip);

		if (ret) {
			printf("failed to queue page flip: %s\n", strerror(errno));
			return -1;
		}

		while (waiting_for_flip) {
			ret = select(_fd + 1, &_fds, NULL, NULL, NULL);
			if (ret < 0) {
				printf("select err: %s\n", strerror(errno));
				return ret;
			} else if (ret == 0) {
				printf("select timeout!\n");
				return -1;
			} else if (FD_ISSET(0, &_fds)) {
				continue;
			}
			drmHandleEvent(_fd, &_evctx);
		}
        return 0;
}

void DirectRenderingManagerImpl::deinit() {
        drmModeRes *resources;
        int i;
        resources = (drmModeRes *)_resource_id;
        for (i = 0; i < resources->count_connectors; i++) {
                drmModeFreeEncoder(_encoder[i]);
                drmModeFreeConnector(_connectors[i]);
        }
        drmModeFreeResources(_resource_id);
        drmClose(_fd);
}
///}}}
//////////////////////////////////////////////////////////
// DirectRenderingManager {{{
DirectRenderingManager::DirectRenderingManager(const char* dev)
    : _impl(NULL)
{
    _impl = new DirectRenderingManagerImpl(dev);
}

DirectRenderingManager::~DirectRenderingManager()
{
    if (_impl) {
        delete _impl;
        _impl = NULL;
    }
}

bool DirectRenderingManager::init() {
    if (!_impl) {
        return false;
    }
    return _impl->init();
}
GenericBufferManager* DirectRenderingManager::createBufferManager() {
    if (!_impl) {
        return NULL;
    }
    return _impl->createBufferManager();
}
unsigned long DirectRenderingManager::getDisplayWidth() const
{
    if (!_impl) {
        return 0;
    }
    return _impl->getDisplayWidth();

}
unsigned long DirectRenderingManager::getDisplayHeight() const
{
    if (!_impl) {
        return 0;
    }
    return _impl->getDisplayHeight();

}

bool DirectRenderingManager::applyMode() {
    if (!_impl) {
        return false;
    }
    return _impl->applyMode();
}
bool DirectRenderingManager::useBufferObject(GenericBufferObject* bo) {
    if (!_impl) {
        return false;
    }
    return _impl->useBufferObject(bo);
}
void    DirectRenderingManager::destroyBufferManager(GenericBufferManager* gbm)
{
    if (_impl) {
        _impl->destroyBufferManager(gbm);
    }
}

void DirectRenderingManager::deinit()
{
    if (!_impl) {
        return;
    }
    return _impl->deinit();
}

bool DirectRenderingManager::flip()
{
    return _impl ? _impl->flip():false;
}

GenericBufferObject* DirectRenderingManager::getBufferObject() 
{ 
    return NULL == _impl ? NULL : _impl->getBufferObject();
}

//}}}
/////////////////////////////////////////////////////////
// GenericBufferManager {{{
//
GenericBufferManager::~GenericBufferManager()
{
}


// }}}
//////////////////////////////////////////////////////////
// KMS {{{

KMS::KMS(const char* dev)
    : _surface(NULL)
    , _gbm(NULL)
    , _drm(NULL)
    , _devName(strdup(dev)) 
{ 
}

KMS::~KMS() {
    deinit();
    if (NULL != _devName) {
        free(_devName);
    }
}

bool KMS::init() {
    if (_drm) {
        deinit();
    }
    _drm = new DirectRenderingManager(_devName);
    if (!_drm->init()) {
        //
        printf("KMS drm initialize failed\n");
        return false;
    }
    _gbm = _drm->createBufferManager();
    if (!_gbm) {
        //
        printf("KMS gbm initialize failed\n");
        return false;
    }
    int w = _drm->getDisplayWidth();
    int h = _drm->getDisplayHeight();
    int format = GBM_FORMAT_XRGB8888;
    int flags  = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    _surface = _gbm->createSurface(w, h, format, flags);
    return true;
}
bool KMS::applyMode() {
    GenericBufferObject* bo = _surface->lockBufferObject();
    assert(_drm->getBufferObject() == NULL);
    _drm->useBufferObject(bo);
    _drm->applyMode();
}

bool KMS::flip()
{
    GenericBufferObject* last_bo = _drm->getBufferObject();
    GenericBufferObject* next_bo = _surface->lockBufferObject();
    bool bResult = false;
    if (_drm && _drm->useBufferObject(next_bo)) {
        bResult = _drm->flip();
        _surface->unlockBufferObject(last_bo); 
    }
    return bResult;
}

EGLNativeDisplayType KMS::getNativeDisplay()
{
    return (EGLNativeDisplayType)_gbm->getNativeDevice();
}

EGLNativeWindowType KMS::getNativeWindow() {
    return(EGLNativeWindowType)( _surface ? _surface->getNativeSurface():0);
}

void KMS::deinit() {
    if (_surface) {
        _gbm->destroySurface(_surface);
        _surface = NULL;
    }
    if (_gbm) {
        _drm->destroyBufferManager(_gbm);
        _gbm = NULL;
    }
    if (_drm) {
        delete _drm;
        _drm = NULL;
    }

}
unsigned long KMS::getDisplayWidth() {
    return _drm ? _drm->getDisplayWidth():0; 
}

unsigned long KMS::getDisplayHeight() {
    return _drm ? _drm->getDisplayHeight():0;
}
//}}}
