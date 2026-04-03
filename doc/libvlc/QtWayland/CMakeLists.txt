cmake_minimum_required(VERSION 3.16)
project(vlc-wayland-player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 COMPONENTS Widgets GuiPrivate REQUIRED)

set(VLC_INSTALL_PATH "" CACHE PATH "Path to VLC installation directory")
set(ENV{PKG_CONFIG_PATH} "${VLC_INSTALL_PATH}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

set(VLC_LIB_PATH "${VLC_INSTALL_PATH}/lib")
set(VLC_INCLUDE_PATH "${VLC_INSTALL_PATH}/include")


include_directories(${VLC_INCLUDE_PATH})
link_directories(${VLC_LIB_PATH})


find_package(PkgConfig REQUIRED)
pkg_check_modules(VLC REQUIRED libvlc)

find_package(Wayland REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

include_directories(${WAYLAND_CLIENT_INCLUDE_DIRS})
link_directories(${WAYLAND_CLIENT_LIBRARY_DIRS})

add_executable(${PROJECT_NAME}
    main.cpp
    vlc_player.h
)

target_link_libraries(${PROJECT_NAME}
    Qt6::Widgets
    Qt6::GuiPrivate
    ${VLC_LIBRARIES}
    ${WAYLAND_CLIENT_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${VLC_INCLUDE_DIRS}
)