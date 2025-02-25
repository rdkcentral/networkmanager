cmake_minimum_required(VERSION 3.3)

project(upnpdiscover)
set(MODULE_NAME upnpdiscover)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBSOUP REQUIRED libsoup-2.4)
pkg_check_modules(GUPNP REQUIRED gupnp-1.0)
pkg_check_modules(GSSDP REQUIRED gssdp-1.2)

add_executable(${MODULE_NAME} UpnpDiscoveryManager.cpp)
target_include_directories(${MODULE_NAME} PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR} ${GLIB_INCLUDE_DIRS} ${LIBSOUP_INCLUDE_DIRS} ${GUPNP_INCLUDE_DIRS} ${GSSDP_INCLUDE_DIRS})
target_link_libraries(${MODULE_NAME} PRIVATE ${GLIB_LIBRARIES} ${LIBSOUP_LIBRARIES} ${GUPNP_LIBRARIES} ${GSSDP_LIBRARIES})
