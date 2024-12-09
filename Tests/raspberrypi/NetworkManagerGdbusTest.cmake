message("building rpi test build")
# Define the project
set(RPI_TEST "NetworkManagerGdbusTest")

# Required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(LIBNM REQUIRED libnm)

# Create the executable target
add_executable(${RPI_TEST}
                NetworkManagerLogger.cpp
                gdbus/NetworkManagerGdbusClient.cpp
                gdbus/NetworkManagerGdbusUtils.cpp
                gdbus/NetworkManagerGdbusMgr.cpp
                gdbus/NetworkManagerGdbusEvent.cpp
                Tests/raspberrypi/NetworkManagerGdbusTest.cpp
)

# Set target properties for C++ standard
set_target_properties(${RPI_TEST} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

# Add compiler options, such as forced include of a specific header
target_compile_options(${RPI_TEST} PRIVATE -g -Wall -include ${CMAKE_SOURCE_DIR}/INetworkManager.h)

target_include_directories(${RPI_TEST} PRIVATE ${GLIB_INCLUDE_DIRS} ${LIBNM_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR})
target_include_directories(${RPI_TEST} PRIVATE gdbus)
# Link libraries to the target
target_link_libraries(${RPI_TEST} ${NAMESPACE}Core::${NAMESPACE}Core ${GLIB_LIBRARIES} ${GIO_LIBRARIES} uuid)

# Include directories
target_include_directories(${RPI_TEST} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Install the executable to the appropriate location
install(TARGETS ${RPI_TEST} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
