#ifdef GP_PLATFORM_LINUX_KMS

#include "Base.h"
#include "Platform.h"
#include "FileSystem.h"
#include "Game.h"
#include "Form.h"
#include "ScriptController.h"
#include <unistd.h>
#include "kms.h"
#include <sys/types.h>
#include <fcntl.h>
#include <libinput.h>
// Externally referenced global variables.
int __argc;
char** __argv;

static bool __initialized;
static bool __suspended;
static EGLDisplay __eglDisplay = EGL_NO_DISPLAY;
static EGLContext __eglContext = EGL_NO_CONTEXT;
static EGLSurface __eglSurface = EGL_NO_SURFACE;
static EGLConfig __eglConfig = 0;
static EGLNativeWindowType __eglWindow = NULL; 

static int __width;
static int __height;
static struct timespec __timespec;
static double __timeStart;
static double __timeAbsolute;
static bool __vsync = WINDOW_VSYNC;
static float __accelRawX;
static float __accelRawY;
static float __accelRawZ;
static float __gyroRawX;
static float __gyroRawY;
static float __gyroRawZ;
static int __orientationAngle = 90;
static bool __multiSampling = false;
static bool __multiTouch = false;
static int __primaryTouchId = -1;
static bool __displayKeyboard = false;

// OpenGL VAO functions.
static const char* __glExtensions;
PFNGLBINDVERTEXARRAYOESPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArrays = NULL;
PFNGLGENVERTEXARRAYSOESPROC glGenVertexArrays = NULL;
PFNGLISVERTEXARRAYOESPROC glIsVertexArray = NULL;

#define GESTURE_TAP_DURATION_MAX			200
#define GESTURE_LONG_TAP_DURATION_MIN   	GESTURE_TAP_DURATION_MAX
#define GESTURE_DRAG_START_DURATION_MIN		GESTURE_LONG_TAP_DURATION_MIN
#define GESTURE_DRAG_DISTANCE_MIN    		30
#define GESTURE_SWIPE_DURATION_MAX      	400
#define GESTURE_SWIPE_DISTANCE_MIN      	50
#define GESTURE_PINCH_DISTANCE_MIN    		GESTURE_DRAG_DISTANCE_MIN

static bool __gestureDraging = false;
static bool __gesturePinching = false;
static std::pair<int, int> __gesturePointer0LastPosition, __gesturePointer1LastPosition;
static std::pair<int, int> __gesturePointer0CurrentPosition, __gesturePointer1CurrentPosition;
static std::pair<int, int> __gesturePinchCentroid;
static int __gesturePointer0Delta, __gesturePointer1Delta;

static std::bitset<6> __gestureEventsProcessed;

struct TouchPointerData
{
    size_t pointerId;
    bool pressed;
    double time;
    int x;
    int y;
};

TouchPointerData __pointer0;
TouchPointerData __pointer1;
static KMS* __kms = NULL;
namespace gameplay
{


static double timespec2millis(struct timespec *a)
{
    GP_ASSERT(a);
    return (1000.0 * a->tv_sec) + (0.000001 * a->tv_nsec);
}

extern void print(const char* format, ...)
{
    GP_ASSERT(format);
    va_list argptr;
    va_start(argptr, format);
    vprintf(format, argptr);
    va_end(argptr);
}

extern int strcmpnocase(const char* s1, const char* s2)
{
    return strcasecmp(s1, s2);
}

static EGLenum checkErrorEGL(const char* msg)
{
    GP_ASSERT(msg);
    static const char* errmsg[] =
    {
        "EGL function succeeded",
        "EGL is not initialized, or could not be initialized, for the specified display",
        "EGL cannot access a requested resource",
        "EGL failed to allocate resources for the requested operation",
        "EGL fail to access an unrecognized attribute or attribute value was passed in an attribute list",
        "EGLConfig argument does not name a valid EGLConfig",
        "EGLContext argument does not name a valid EGLContext",
        "EGL current surface of the calling thread is no longer valid",
        "EGLDisplay argument does not name a valid EGLDisplay",
        "EGL arguments are inconsistent",
        "EGLNativePixmapType argument does not refer to a valid native pixmap",
        "EGLNativeWindowType argument does not refer to a valid native window",
        "EGL one or more argument values are invalid",
        "EGLSurface argument does not name a valid surface configured for rendering",
        "EGL power management event has occurred",
    };
    EGLenum error = eglGetError();
    print("%s: %s.", msg, errmsg[error - EGL_SUCCESS]);
    return error;
}

static int getRotation()
{
    int rotation;

    return rotation;
}
class InputManager
{
public:
    InputManager();
    ~InputManager();
    
    bool init();
    bool dispatch();
protected:
    static int open_restrict(const char* dev, int flags, void* userdata);
    static void close_restrict(int, void* userdata);
	bool onDeviceEvent(int type, libinput_event* evt);
	bool onKeyboardEvent(libinput_event* evt);
	bool onPointerEvent(int type, libinput_event* evt);
	bool onTouchEvent(int type, libinput_event* evt);
	double getCurrentTicket();
private:
    libinput* _li;
    libinput_device* _lidev;
    static libinput_interface _interface;
    int     _lastx;
    int     _lasty;
    int     _lastslot;
    double  _last_emit;
};

libinput_interface InputManager::_interface = {
    &InputManager::open_restrict,
    &InputManager::close_restrict
};
int InputManager::open_restrict(const char* devNode, int flags, void* userdata)
{
    return open(devNode, flags);
}

void InputManager::close_restrict(int dev, void* userdata) {
    close(dev);
}

InputManager::InputManager()
: _li(NULL)
, _lidev(NULL)
, _lastx(0)
, _lasty(0)
, _lastslot(0)
, _last_emit(0)
{
}

InputManager::~InputManager()
{
    if (_lidev) {
        libinput_device_unref(_lidev);
        _lidev = NULL;
    }
    if (_li) {
        libinput_unref(_li);
        _li = NULL;
    }
}

double InputManager::getCurrentTicket() {
    timespec ts;
     clock_gettime(CLOCK_REALTIME, &ts);
    return timespec2millis(&ts);
}

bool InputManager::init() {
    bool b =false;
    do
    {
        _li = libinput_path_create_context(&_interface, NULL);
        if (!_li) {
            break;
        }
        const char* dev="/dev/input/event0";
        _lidev = libinput_path_add_device(_li, dev);
        if (!_lidev) {
            break;
        }
        b = true; 
        _last_emit = getCurrentTicket();
    } while(false);
    if (!b && _li) {
        libinput_unref(_li);
        _li = NULL;
    }
    return b;
}

bool InputManager::dispatch() {
    if (libinput_dispatch(_li)) {
        return false;
    }
    
    bool ret = true; 
    int type = LIBINPUT_EVENT_NONE;
    while (LIBINPUT_EVENT_NONE != (type =libinput_next_event_type(_li))) {
	    if (LIBINPUT_EVENT_NONE == type) {
	        return true;
	    }
	    
	    libinput_event* evt = libinput_get_event(_li);
	    if (! evt) {
	       return false;
	    } 
	    switch (type)
	    {
	        case LIBINPUT_EVENT_DEVICE_ADDED:
			case LIBINPUT_EVENT_DEVICE_REMOVED:
	            ret = onDeviceEvent(type, evt);
	            break;
			case LIBINPUT_EVENT_KEYBOARD_KEY:
	            ret = onKeyboardEvent(evt);
	            break;
			case LIBINPUT_EVENT_POINTER_MOTION:
			case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
			case LIBINPUT_EVENT_POINTER_BUTTON:
			case LIBINPUT_EVENT_POINTER_AXIS:
	            ret = onPointerEvent(type, evt);
	            break;
			case LIBINPUT_EVENT_TOUCH_DOWN:
			case LIBINPUT_EVENT_TOUCH_UP:
			case LIBINPUT_EVENT_TOUCH_MOTION:
			case LIBINPUT_EVENT_TOUCH_CANCEL:
			case LIBINPUT_EVENT_TOUCH_FRAME:
	            ret = onTouchEvent(type, evt);
	            break;
	        default:
	            ret = false; // keep false
	    }
	    libinput_event_destroy(evt);
    }
    return ret;
}
bool InputManager::onDeviceEvent(int type, libinput_event* evt)
{
    return true;
}
bool InputManager::onKeyboardEvent(libinput_event* evt) {
    return true;
}
bool InputManager::onPointerEvent(int type, libinput_event* evt) {
    return true;
}

bool InputManager::onTouchEvent(int type, libinput_event* evt) {
    
    Touch::TouchEvent te = Touch::TOUCH_MOVE;
    libinput_event_touch* e = (libinput_event_touch*)evt;
    if (!e) {
        return false;
    }
    int x = libinput_event_touch_get_x_transformed(e, __width);
    int y = libinput_event_touch_get_y_transformed(e, __height);
    int slot = libinput_event_touch_get_slot(e); 

    switch (type)
    {
       	case LIBINPUT_EVENT_TOUCH_DOWN:
            te = Touch::TOUCH_PRESS;
            _lastx = x;
            _lasty = y;
            _lastslot = slot;
            break;
		case LIBINPUT_EVENT_TOUCH_UP:
            te = Touch::TOUCH_RELEASE;
            x = _lastx;
            y = _lasty;
            slot = _lastslot;
            break;
		case LIBINPUT_EVENT_TOUCH_MOTION:
            te = Touch::TOUCH_MOVE;
            _lastx = x;
            _lasty = y;
            _lastslot = slot;
            if (getCurrentTicket() - _last_emit < 100) {
                return true;
            }
            _last_emit = getCurrentTicket();
            break;
		case LIBINPUT_EVENT_TOUCH_CANCEL:
		case LIBINPUT_EVENT_TOUCH_FRAME:
            return true;
            break;
        default:
            return false;
    }
    //printf("type:%08x, x=%d, y=%d, slot=%d\n", te, x, y, slot);
    Platform::touchEventInternal(te, 
	    x, 
	    y,
	    slot,
	    false);
   
    return true;
}

InputManager __im;
// Initialized EGL resources.
static bool initEGL()
{
    int samples = 0;
    Properties* config = Game::getInstance()->getConfig()->getNamespace("window", true);
    if (config)
    {
        samples = std::max(config->getInt("samples"), 0);
    }

    // Hard-coded to 32-bit/OpenGL ES 2.0.
    // NOTE: EGL_SAMPLE_BUFFERS, EGL_SAMPLES and EGL_DEPTH_SIZE MUST remain at the beginning of the attribute list
    // since they are expected to be at indices 0-5 in config fallback code later.
    // EGL_DEPTH_SIZE is also expected to
    EGLint eglConfigAttrs[] =
    {
        EGL_SAMPLE_BUFFERS,     samples > 0 ? 1 : 0,
        EGL_SAMPLES,            samples,
        EGL_DEPTH_SIZE,         24,
        EGL_RED_SIZE,           8,
        EGL_GREEN_SIZE,         8,
        EGL_BLUE_SIZE,          8,
        EGL_ALPHA_SIZE,         8,
        EGL_STENCIL_SIZE,       8,
        EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    __multiSampling = samples > 0;
    
    EGLint eglConfigCount;
    const EGLint eglContextAttrs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION,    2,
        EGL_NONE
    };

    const EGLint eglSurfaceAttrs[] =
    {
        EGL_RENDER_BUFFER,    EGL_BACK_BUFFER,
        EGL_NONE
    };

    if (__eglDisplay == EGL_NO_DISPLAY && __eglContext == EGL_NO_CONTEXT)
    {
        // Get the EGL display and initialize.
        __eglDisplay = eglGetDisplay(__kms->getNativeDisplay());
        if (__eglDisplay == EGL_NO_DISPLAY)
        {
            checkErrorEGL("eglGetDisplay");
            goto error;
        }

        if (eglInitialize(__eglDisplay, NULL, NULL) != EGL_TRUE)
        {
            checkErrorEGL("eglInitialize");
            goto error;
        }

        // Try both 24 and 16-bit depth sizes since some hardware (i.e. Tegra) does not support 24-bit depth
        bool validConfig = false;
        EGLint depthSizes[] = { 24, 16 };
        for (unsigned int i = 0; i < 2; ++i)
        {
            eglConfigAttrs[1] = samples > 0 ? 1 : 0;
            eglConfigAttrs[3] = samples;
            eglConfigAttrs[5] = depthSizes[i];

            if (eglChooseConfig(__eglDisplay, 
                eglConfigAttrs,
                &__eglConfig, 
                1, 
                &eglConfigCount) == EGL_TRUE && eglConfigCount > 0)
            {
                validConfig = true;
                break;
            }

            if (samples)
            {
                // Try lowering the MSAA sample size until we find a  config
                int sampleCount = samples;
                while (sampleCount)
                {
                    GP_WARN("No EGL config found for depth_size=%d and samples=%d. Trying samples=%d instead.",
                        depthSizes[i],
                        sampleCount,
                        sampleCount / 2);
                    sampleCount /= 2;
                    eglConfigAttrs[1] = sampleCount > 0 ? 1 : 0;
                    eglConfigAttrs[3] = sampleCount;
                    if (eglChooseConfig(__eglDisplay, 
                            eglConfigAttrs,
                            &__eglConfig, 
                            1,
                            &eglConfigCount) == EGL_TRUE && eglConfigCount > 0)
                    {
                        validConfig = true;
                        break;
                    }
                }

                __multiSampling = sampleCount > 0;

                if (validConfig)
                    break;
            }
            else
            {
                GP_WARN("No EGL config found for depth_size=%d.", depthSizes[i]);
            }
        }

        if (!validConfig)
        {
            checkErrorEGL("eglChooseConfig");
            goto error;
        }

        __eglContext = eglCreateContext(__eglDisplay, __eglConfig, EGL_NO_CONTEXT, eglContextAttrs);
        if (__eglContext == EGL_NO_CONTEXT)
        {
            checkErrorEGL("eglCreateContext");
            goto error;
        }
    }
    
    // EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
    // guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
    // As soon as we picked a EGLConfig, we can safely reconfigure the
    // ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID.
    EGLint format;
    eglGetConfigAttrib(__eglDisplay, __eglConfig, EGL_NATIVE_VISUAL_ID, &format);
    __eglSurface = eglCreateWindowSurface(__eglDisplay, __eglConfig, __eglWindow, eglSurfaceAttrs);
    if (__eglSurface == EGL_NO_SURFACE)
    {
        checkErrorEGL("eglCreateWindowSurface");
        goto error;
    }
    
    if (eglMakeCurrent(__eglDisplay, __eglSurface, __eglSurface, __eglContext) != EGL_TRUE)
    {
        checkErrorEGL("eglMakeCurrent");
        goto error;
    }
    
    eglQuerySurface(__eglDisplay, __eglSurface, EGL_WIDTH, &__width);
    eglQuerySurface(__eglDisplay, __eglSurface, EGL_HEIGHT, &__height);

    __orientationAngle = getRotation() * 90;
    
    // Set vsync.
    eglSwapInterval(__eglDisplay, WINDOW_VSYNC ? 1 : 0);
    
    // Initialize OpenGL ES extensions.
    __glExtensions = (const char*)glGetString(GL_EXTENSIONS);
    
    if (strstr(__glExtensions, "GL_OES_vertex_array_object") || strstr(__glExtensions, "GL_ARB_vertex_array_object"))
    {
        // Disable VAO extension for now.
        glBindVertexArray = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress("glBindVertexArrayOES");
        glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSOESPROC)eglGetProcAddress("glDeleteVertexArraysOES");
        glGenVertexArrays = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress("glGenVertexArraysOES");
        glIsVertexArray = (PFNGLISVERTEXARRAYOESPROC)eglGetProcAddress("glIsVertexArrayOES");
    }

    if (!__kms->applyMode()) {
        printf("__kms applyMode failed\n");
        goto error;
    }

    return true;
    
error:
    return false;
}

static void destroyEGLSurface()
{
    if (__eglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(__eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (__eglSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(__eglDisplay, __eglSurface);
        __eglSurface = EGL_NO_SURFACE;
    }
}

static void destroyEGLMain()
{
    destroyEGLSurface();

    if (__eglContext != EGL_NO_CONTEXT)
    {
        eglDestroyContext(__eglDisplay, __eglContext);
        __eglContext = EGL_NO_CONTEXT;
    }

    if (__eglDisplay != EGL_NO_DISPLAY)
    {
        eglTerminate(__eglDisplay);
        __eglDisplay = EGL_NO_DISPLAY;
    }
}
// Gets the Keyboard::Key enumeration constant that corresponds to the given Android key code.
static Keyboard::Key getKey(int keycode, int metastate)
{
    
    switch(keycode)
    {
        default:
            return Keyboard::KEY_NONE;
    }
}

/**
 * Returns the unicode value for the given keycode or zero if the key is not a valid printable character.
 */
static int getUnicode(int keycode, int metastate)
{
    // TODO: Doesn't support unicode currently.
    Keyboard::Key key = getKey(keycode, metastate);
    switch (key)
    {
    case Keyboard::KEY_BACKSPACE:
        return 0x0008;
    case Keyboard::KEY_TAB:
        return 0x0009;
    case Keyboard::KEY_RETURN:
    case Keyboard::KEY_KP_ENTER:
        return 0x000A;
    case Keyboard::KEY_ESCAPE:
        return 0x001B;
    case Keyboard::KEY_SPACE:
    case Keyboard::KEY_EXCLAM:
    case Keyboard::KEY_QUOTE:
    case Keyboard::KEY_NUMBER:
    case Keyboard::KEY_DOLLAR:
    case Keyboard::KEY_PERCENT:
    case Keyboard::KEY_CIRCUMFLEX:
    case Keyboard::KEY_AMPERSAND:
    case Keyboard::KEY_APOSTROPHE:
    case Keyboard::KEY_LEFT_PARENTHESIS:
    case Keyboard::KEY_RIGHT_PARENTHESIS:
    case Keyboard::KEY_ASTERISK:
    case Keyboard::KEY_PLUS:
    case Keyboard::KEY_COMMA:
    case Keyboard::KEY_MINUS:
    case Keyboard::KEY_PERIOD:
    case Keyboard::KEY_SLASH:
    case Keyboard::KEY_ZERO:
    case Keyboard::KEY_ONE:
    case Keyboard::KEY_TWO:
    case Keyboard::KEY_THREE:
    case Keyboard::KEY_FOUR:
    case Keyboard::KEY_FIVE:
    case Keyboard::KEY_SIX:
    case Keyboard::KEY_SEVEN:
    case Keyboard::KEY_EIGHT:
    case Keyboard::KEY_NINE:
    case Keyboard::KEY_COLON:
    case Keyboard::KEY_SEMICOLON:
    case Keyboard::KEY_LESS_THAN:
    case Keyboard::KEY_EQUAL:
    case Keyboard::KEY_GREATER_THAN:
    case Keyboard::KEY_QUESTION:
    case Keyboard::KEY_AT:
    case Keyboard::KEY_CAPITAL_A:
    case Keyboard::KEY_CAPITAL_B:
    case Keyboard::KEY_CAPITAL_C:
    case Keyboard::KEY_CAPITAL_D:
    case Keyboard::KEY_CAPITAL_E:
    case Keyboard::KEY_CAPITAL_F:
    case Keyboard::KEY_CAPITAL_G:
    case Keyboard::KEY_CAPITAL_H:
    case Keyboard::KEY_CAPITAL_I:
    case Keyboard::KEY_CAPITAL_J:
    case Keyboard::KEY_CAPITAL_K:
    case Keyboard::KEY_CAPITAL_L:
    case Keyboard::KEY_CAPITAL_M:
    case Keyboard::KEY_CAPITAL_N:
    case Keyboard::KEY_CAPITAL_O:
    case Keyboard::KEY_CAPITAL_P:
    case Keyboard::KEY_CAPITAL_Q:
    case Keyboard::KEY_CAPITAL_R:
    case Keyboard::KEY_CAPITAL_S:
    case Keyboard::KEY_CAPITAL_T:
    case Keyboard::KEY_CAPITAL_U:
    case Keyboard::KEY_CAPITAL_V:
    case Keyboard::KEY_CAPITAL_W:
    case Keyboard::KEY_CAPITAL_X:
    case Keyboard::KEY_CAPITAL_Y:
    case Keyboard::KEY_CAPITAL_Z:
    case Keyboard::KEY_LEFT_BRACKET:
    case Keyboard::KEY_BACK_SLASH:
    case Keyboard::KEY_RIGHT_BRACKET:
    case Keyboard::KEY_UNDERSCORE:
    case Keyboard::KEY_GRAVE:
    case Keyboard::KEY_A:
    case Keyboard::KEY_B:
    case Keyboard::KEY_C:
    case Keyboard::KEY_D:
    case Keyboard::KEY_E:
    case Keyboard::KEY_F:
    case Keyboard::KEY_G:
    case Keyboard::KEY_H:
    case Keyboard::KEY_I:
    case Keyboard::KEY_J:
    case Keyboard::KEY_K:
    case Keyboard::KEY_L:
    case Keyboard::KEY_M:
    case Keyboard::KEY_N:
    case Keyboard::KEY_O:
    case Keyboard::KEY_P:
    case Keyboard::KEY_Q:
    case Keyboard::KEY_R:
    case Keyboard::KEY_S:
    case Keyboard::KEY_T:
    case Keyboard::KEY_U:
    case Keyboard::KEY_V:
    case Keyboard::KEY_W:
    case Keyboard::KEY_X:
    case Keyboard::KEY_Y:
    case Keyboard::KEY_Z:
    case Keyboard::KEY_LEFT_BRACE:
    case Keyboard::KEY_BAR:
    case Keyboard::KEY_RIGHT_BRACE:
    case Keyboard::KEY_TILDE:
        return key;
    default:
        return 0;
    }
}

Platform::Platform(Game* game)
    : _game(game)
{
}

Platform::~Platform()
{
}

Platform* Platform::create(Game* game)
{
    __initialized = false;
    do 
    {
        if (NULL == __kms) {
            __kms = new KMS("/dev/dri/card0");
            if (!__kms->init() ) {
                printf("init kms failed\n");
                break;
            }
            __eglWindow = __kms->getNativeWindow();
            __width     = __kms->getDisplayWidth();
            __height    = __kms->getDisplayHeight();

            if (!initEGL()) {
                printf("init gl failed\n");
                break;
            }
        }
        if (!__im.init()) {
            break;
        }
        __initialized = true;
    } while (false);

    if (__initialized) {
        Platform* platform = new Platform(game);
        return platform;
    }

    if (__kms) {
        delete __kms;
    }
    return NULL;
}

int Platform::enterMessagePump()
{

    __initialized = __eglWindow != NULL;
    __suspended = false;
   // Get the initial time.
    clock_gettime(CLOCK_REALTIME, &__timespec);
    __timeStart = timespec2millis(&__timespec);
    __timeAbsolute = 0L;

    _game->run();

    while (true)
    {
        // Read all pending events.
        int ident;
        int events;
        // Idle time (no events left to process) is spent rendering.
        // We skip rendering when the app is paused.
        if (__initialized && !__suspended)
        {
            _game->frame();

            // Post the new frame to the display.
            // Note that there are a couple cases where eglSwapBuffers could fail
            // with an error code that requires a certain level of re-initialization:
            //
            // 1) EGL_BAD_NATIVE_WINDOW - Called when the surface we're currently using
            //    is invalidated. This would require us to destroy our EGL surface,
            //    close our OpenKODE window, and start again.
            //
            // 2) EGL_CONTEXT_LOST - Power management event that led to our EGL context
            //    being lost. Requires us to re-create and re-initalize our EGL context
            //    and all OpenGL ES state.
            //
            // For now, if we get these, we'll simply exit.
            int rc = eglSwapBuffers(__eglDisplay, __eglSurface);
            if (rc != EGL_TRUE)
            {
                EGLint error = eglGetError();
                if (error == EGL_BAD_NATIVE_WINDOW)
                {
                    if (__eglWindow != NULL)
                    {
                        destroyEGLSurface();
                        initEGL();
                    }
                    __initialized = true;
                }
                else
                {
                    perror("eglSwapBuffers");
                    break;
                }
            } else {
                __kms->flip();
            }
            if (!__im.dispatch()) {
                printf("im dispatch error !\n");
            }
        }
            
        // Display the keyboard.
    }
}

void Platform::signalShutdown() 
{
    // nothing to do  
}

bool Platform::canExit()
{
    return true;
}
   
unsigned int Platform::getDisplayWidth()
{
    return __width;
}
    
unsigned int Platform::getDisplayHeight()
{
    return __height;
}
    
double Platform::getAbsoluteTime()
{
    clock_gettime(CLOCK_REALTIME, &__timespec);
    double now = timespec2millis(&__timespec);
    __timeAbsolute = now - __timeStart;

    return __timeAbsolute;
}

void Platform::setAbsoluteTime(double time)
{
    __timeAbsolute = time;
}

bool Platform::isVsync()
{
    return __vsync;
}

void Platform::setVsync(bool enable)
{
    eglSwapInterval(__eglDisplay, enable ? 1 : 0);
    __vsync = enable;
}


void Platform::swapBuffers()
{
    if (__eglDisplay && __eglSurface)
        eglSwapBuffers(__eglDisplay, __eglSurface);
}

void Platform::sleep(long ms)
{
    usleep(ms * 1000);
}

void Platform::setMultiSampling(bool enabled)
{
    if (enabled == __multiSampling)
    {
        return;
    }

    // TODO
    __multiSampling = enabled;
}

bool Platform::isMultiSampling()
{
    return __multiSampling;
}

void Platform::setMultiTouch(bool enabled)
{
    __multiTouch = enabled;
}

bool Platform::isMultiTouch()
{
    return __multiTouch;
}

bool Platform::hasAccelerometer()
{
    return true;
}

void Platform::getAccelerometerValues(float* pitch, float* roll)
{
    double tx, ty, tz;
    
    // By default, android accelerometer values are oriented to the portrait mode.
    // flipping the x and y to get the desired landscape mode values.
    switch (__orientationAngle)
    {
    case 90:
        tx = -__accelRawY;
        ty = __accelRawX;
        break;
    case 180:
        tx = -__accelRawX;
        ty = -__accelRawY;
        break;
    case 270:
        tx = __accelRawY;
        ty = -__accelRawX;
        break;
    default:
        tx = __accelRawX;
        ty = __accelRawY;
        break;
    }
    tz = __accelRawZ;

    if (pitch != NULL)
    {
        GP_ASSERT(tx * tx + tz * tz);
        *pitch = -atan(ty / sqrt(tx * tx + tz * tz)) * 180.0f * M_1_PI;
    }
    if (roll != NULL)
    {
        GP_ASSERT(ty * ty + tz * tz);
        *roll = -atan(tx / sqrt(ty * ty + tz * tz)) * 180.0f * M_1_PI;
    }
}

void Platform::getSensorValues(float* accelX, float* accelY, float* accelZ, float* gyroX, float* gyroY, float* gyroZ)
{
    if (accelX)
    {
        *accelX = __accelRawX;
    }

    if (accelY)
    {
        *accelY = __accelRawY;
    }

    if (accelZ)
    {
        *accelZ = __accelRawZ;
    }

    if (gyroX)
    {
        *gyroX = __gyroRawX;
    }

    if (gyroY)
    {
        *gyroY = __gyroRawY;
    }

    if (gyroZ)
    {
        *gyroZ = __gyroRawZ;
    }
}

void Platform::getArguments(int* argc, char*** argv)
{
    if (argc)
        *argc = 0;
    if (argv)
        *argv = 0;
}

bool Platform::hasMouse()
{
    // not 
    return false;
}

void Platform::setMouseCaptured(bool captured)
{
    // not 
}

bool Platform::isMouseCaptured()
{
    // not 
    return false;
}

void Platform::setCursorVisible(bool visible)
{
    // not 
}

bool Platform::isCursorVisible()
{
    // not 
    return false;
}

void Platform::displayKeyboard(bool display)
{
    if (display)
        __displayKeyboard = true;
    else
        __displayKeyboard = false;
}

void Platform::shutdownInternal()
{
    Game::getInstance()->shutdown();
}

bool Platform::isGestureSupported(Gesture::GestureEvent evt)
{
    // Pinch currently not implemented
    return evt == gameplay::Gesture::GESTURE_SWIPE || evt == gameplay::Gesture::GESTURE_TAP || evt == gameplay::Gesture::GESTURE_LONG_TAP ||
        evt == gameplay::Gesture::GESTURE_DRAG || evt == gameplay::Gesture::GESTURE_DROP || evt == gameplay::Gesture::GESTURE_PINCH;
}

void Platform::registerGesture(Gesture::GestureEvent evt)
{
    switch(evt)
    {
    case Gesture::GESTURE_ANY_SUPPORTED:
        __gestureEventsProcessed.set();
        break;

    case Gesture::GESTURE_TAP:
    case Gesture::GESTURE_SWIPE:
    case Gesture::GESTURE_LONG_TAP:
    case Gesture::GESTURE_DRAG:
    case Gesture::GESTURE_DROP:
    case Gesture::GESTURE_PINCH:
        __gestureEventsProcessed.set(evt);
        break;

    default:
        break;
    }
}

void Platform::unregisterGesture(Gesture::GestureEvent evt)
{
    switch(evt)
    {
    case Gesture::GESTURE_ANY_SUPPORTED:
        __gestureEventsProcessed.reset();
        break;

    case Gesture::GESTURE_TAP:
    case Gesture::GESTURE_SWIPE:
    case Gesture::GESTURE_LONG_TAP:
    case Gesture::GESTURE_DRAG:
    case Gesture::GESTURE_DROP:
        __gestureEventsProcessed.set(evt, 0);
        break;

    default:
        break;
    }
}
    
bool Platform::isGestureRegistered(Gesture::GestureEvent evt)
{
    return __gestureEventsProcessed.test(evt);
}

void Platform::pollGamepadState(Gamepad* gamepad)
{
}

bool Platform::launchURL(const char *url)
{
    if (url == NULL || *url == '\0')
        return false;

    bool result = true;

    return result;
}

std::string Platform::displayFileDialog(size_t mode, const char* title, const char* filterDescription, const char* filterExtensions, const char* initialDirectory)
{
    return "";
}

}

#endif
