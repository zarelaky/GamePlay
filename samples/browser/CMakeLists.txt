
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
    include_directories(${EGL_INCLUDE_DIRS})
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

set( GAME_NAME sample-browser)

set(GAME_SRC
    src/Audio3DSample.cpp
    src/Audio3DSample.h
    src/BillboardSample.cpp
    src/BillboardSample.h
    src/CreateSceneSample.cpp
    src/CreateSceneSample.h    
    src/CreateSceneSample.h
    src/FirstPersonCamera.cpp
    src/FirstPersonCamera.h
    src/FormsSample.cpp
    src/FormsSample.h
    src/GamepadSample.cpp
    src/GamepadSample.h
    src/GestureSample.cpp
    src/GestureSample.h
    src/Grid.cpp
    src/Grid.h
    src/InputSample.cpp
    src/InputSample.h
    src/LightSample.cpp
    src/LightSample.h
    src/LoadSceneSample.cpp
    src/LoadSceneSample.h
    src/MeshBatchSample.cpp
    src/MeshBatchSample.h
    src/MeshPrimitiveSample.cpp
    src/MeshPrimitiveSample.h
    src/PhysicsCollisionObjectSample.cpp
    src/PhysicsCollisionObjectSample.h
    src/PostProcessSample.cpp
    src/PostProcessSample.h
    src/SpriteBatchSample.cpp
    src/SpriteBatchSample.h
    src/TerrainSample.cpp
    src/TerrainSample.h
    src/Sample.cpp
    src/Sample.h
    src/SamplesGame.cpp
    src/SamplesGame.h
    src/TextSample.cpp
    src/TextSample.h
    src/TextureSample.cpp
    src/TextureSample.h
    src/TriangleSample.cpp
    src/TriangleSample.h
)

add_executable(${GAME_NAME}
    ${GAME_SRC}
)

target_link_libraries(${GAME_NAME} ${GAMEPLAY_LIBRARIES})

set_target_properties(${GAME_NAME} PROPERTIES
    OUTPUT_NAME "${GAME_NAME}"
    CLEAN_DIRECT_OUTPUT 1
)

source_group(res FILES ${GAME_RES} ${GAMEPLAY_RES} ${GAMEPLAY_RES_SHADERS} ${GAMEPLAY_RES_UI})
source_group(src FILES ${GAME_SRC})

COPY_RES( ${GAME_NAME} )
COPY_RES_EXTRA( ${GAME_NAME} ${CMAKE_SOURCE_DIR}/gameplay
    res/logo_powered_white.png 
    res/shaders/*
    res/ui/*
)

