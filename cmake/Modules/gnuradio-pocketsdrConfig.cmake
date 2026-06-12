find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_POCKETSDR gnuradio-pocketsdr)

FIND_PATH(
    GR_POCKETSDR_INCLUDE_DIRS
    NAMES gnuradio/pocketsdr/api.h
    HINTS $ENV{POCKETSDR_DIR}/include
        ${PC_POCKETSDR_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_POCKETSDR_LIBRARIES
    NAMES gnuradio-pocketsdr
    HINTS $ENV{POCKETSDR_DIR}/lib
        ${PC_POCKETSDR_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-pocketsdrTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_POCKETSDR DEFAULT_MSG GR_POCKETSDR_LIBRARIES GR_POCKETSDR_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_POCKETSDR_LIBRARIES GR_POCKETSDR_INCLUDE_DIRS)
