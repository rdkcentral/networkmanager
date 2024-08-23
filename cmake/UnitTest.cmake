message("Building for unit tests...")

message("Generating empty headers to suppress compiler errors")

file(GLOB BASEDIR Tests)
set(BASEDIR ${BASEDIR}/headers)
set(EMPTY_HEADERS_DIRS
        ${BASEDIR}
        ${BASEDIR}/rdk/iarmbus
        ${BASEDIR}/network
        )

set(EMPTY_HEADERS
        ${BASEDIR}/rdk/iarmbus/libIARM.h
        ${BASEDIR}/rdk/iarmbus/libIBus.h
        ${BASEDIR}/rdk/iarmbus/libIBusDaemon.h
        ${BASEDIR}/network/wifiSrvMgrIarmIf.h
        ${BASEDIR}/network/netsrvmgrIarm.h
        )

file(MAKE_DIRECTORY ${EMPTY_HEADERS_DIRS})

file(GLOB_RECURSE EMPTY_HEADERS_AVAILABLE "${BASEDIR}/*")
if (EMPTY_HEADERS_AVAILABLE)
    message("Skip already generated headers to avoid rebuild")
    list(REMOVE_ITEM EMPTY_HEADERS ${EMPTY_HEADERS_AVAILABLE})
endif ()
if (EMPTY_HEADERS)
    file(TOUCH ${EMPTY_HEADERS})
endif ()

include_directories(${EMPTY_HEADERS_DIRS})

message("Adding compiler and linker options for all targets")

file(GLOB BASEDIR Tests/mocks)
set(FAKE_HEADERS
        ${BASEDIR}/Iarm.h
        )

foreach (file ${FAKE_HEADERS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include ${file}")
endforeach ()

add_compile_options(-Wall -Werror)

add_link_options(-Wl,-wrap,system -Wl,-wrap,popen -Wl,-wrap,syslog -Wl,-wrap,pclose -Wl,-wrap,getmntent -Wl,-wrap,setmntent -Wl,-wrap,v_secure_popen -Wl,-wrap,v_secure_pclose -Wl,-wrap,v_secure_system)

message("Setting build options")
set(CMAKE_DISABLE_FIND_PACKAGE_IARMBus ON)
