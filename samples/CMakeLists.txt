include(BuildHelpers.CMakeLists.txt)

include_directories( 
    ${CMAKE_SOURCE_DIR}/gameplay/src
    ${CMAKE_SOURCE_DIR}/external-deps/lua/include
    ${CMAKE_SOURCE_DIR}/external-deps/bullet/include
    ${CMAKE_SOURCE_DIR}/external-deps/png/include
    ${CMAKE_SOURCE_DIR}/external-deps/oggvorbis/include
    ${CMAKE_SOURCE_DIR}/external-deps/zlib/include
    ${CMAKE_SOURCE_DIR}/external-deps/openal/include
    ${CMAKE_SOURCE_DIR}/external-deps/glew/include
)

IF( ${PLATFORM} MATCHES "arm")
    add_definitions(-DGP_PLATFORM_LINUX_KMS)
ELSE(${PLATFORM} MATCHES "arm")
    add_definitions(-DGP_PLATFORM_LINUX) 
ENDIF(${PLATFORM} MATCHES "arm")

link_directories(
    ${CMAKE_SOURCE_DIR}/external-deps/lua/lib/linux/${ARCH_DIR}
    ${CMAKE_SOURCE_DIR}/external-deps/zlib/lib/linux/${ARCH_DIR}
    ${CMAKE_SOURCE_DIR}/external-deps/png/lib/linux/${ARCH_DIR}
    ${CMAKE_SOURCE_DIR}/external-deps/bullet/lib/linux/${ARCH_DIR}
    ${CMAKE_SOURCE_DIR}/external-deps/oggvorbis/lib/linux/${ARCH_DIR}
    ${CMAKE_SOURCE_DIR}/external-deps/openal/lib/linux/${ARCH_DIR}
    ${CMAKE_SOURCE_DIR}/external-deps/glew/lib/linux/${ARCH_DIR}
)

set(GAMEPLAY_LIBRARIES
    gameplay
    m
    lua
    png
    z
    vorbis
    vorbisfile
    ogg
    BulletDynamics
    BulletCollision
    LinearMath
    openal
    rt
    dl
    pthread
    glib-2.0
    gobject-2.0
) 

IF (${PLATFORM} MATCHES "arm")
    IF (!EGL_FOUND)
        pkg_check_modules(EGL REQUIRED egl)
    ENDIF (!EGL_FOUND)

    IF (!GLESv2_FOUND)
        pkg_check_modules(GLESv2 REQUIRED glesv2)
    ENDIF (!GLESv2_FOUND)
   
    IF (!GBM_FOUND) 
        pkg_check_modules(GBM REQUIRED gbm)
    ENDIF()

    IF (!DRM_FOUND)
        pkg_check_modules(DRM REQUIRED libdrm)
    ENDIF()

    IF (!UDEV_FOUND) 
        pkg_check_modules(UDEV REQUIRED libudev)
    ENDIF()

    IF (!EVDEV_FOUND) 
        pkg_check_modules(EVDEV REQUIRED libevdev)
    ENDIF()
    
    IF (!MTDEV_FOUND) 
        pkg_check_modules(MTDEV REQUIRED mtdev)
    ENDIF()

    IF (!LIBINPUT_FOUND) 
        pkg_check_modules(LIBINPUT REQUIRED libinput)
    ENDIF()

    SET(GAMEPLAY_LIBRARIES
        ${GAMEPLAY_LIBRARIES} 
        ${GLESv2_LDFLAGS}
        ${EGL_LDFLAGS}
        ${GBM_LDFLAGS}
        ${DRM_LDFLAGS}
        ${UDEV_LDFLAGS}
        ${EVDEV_LDFLAGS}
        ${MTDEV_LDFLAGS}
        ${LIBINPUT_LDFLAGS}
        )

    MESSAGE(INFO " building with arm")
ELSE ( !${PLATFORM} MATCHES "arm")
   set(GAMEPLAY_LIBRARIES ${GAMEPLAY_LIBRARIES} GLEW GL X11 gtk-x11-2.0)
   MESSAGE(INFO " building with x11")
ENDIF(${PLATFORM} MATCHES "arm")


add_subdirectory(browser)
add_subdirectory(character)
add_subdirectory(lua)
add_subdirectory(mesh)
add_subdirectory(particles)
add_subdirectory(racer)
add_subdirectory(spaceship)
