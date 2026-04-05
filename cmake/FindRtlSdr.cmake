# FindRtlSdr.cmake
# Find the RTL-SDR library (librtlsdr)
#
# Sets:
#   RTLSDR_FOUND        - True if librtlsdr is found
#   RTLSDR_INCLUDE_DIRS - Include directories
#   RTLSDR_LIBRARIES    - Libraries to link

find_path(RTLSDR_INCLUDE_DIR
    NAMES rtl-sdr.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        $ENV{RTLSDR_DIR}/include
)

find_library(RTLSDR_LIBRARY
    NAMES rtlsdr
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        $ENV{RTLSDR_DIR}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RtlSdr
    REQUIRED_VARS RTLSDR_LIBRARY RTLSDR_INCLUDE_DIR
)

if(RTLSDR_FOUND)
    set(RTLSDR_INCLUDE_DIRS ${RTLSDR_INCLUDE_DIR})
    set(RTLSDR_LIBRARIES ${RTLSDR_LIBRARY})
endif()

mark_as_advanced(RTLSDR_INCLUDE_DIR RTLSDR_LIBRARY)
