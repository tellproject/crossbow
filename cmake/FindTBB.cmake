# - Find TBB

# Look for the TBB header
find_path(TBB_INCLUDE_DIR
        NAMES tbb/tbb.h
        HINTS ${TBB_ROOT} ENV TBB_ROOT
        PATH_SUFFIXES include)

# Look for the TBB library
find_library(TBB_LIBRARY
        NAMES tbb
        HINTS ${TBB_ROOT} ENV TBB_ROOT
        PATH_SUFFIXES lib)

set(TBB_LIBRARIES ${TBB_LIBRARY})
set(TBB_INCLUDE_DIRS ${TBB_INCLUDE_DIR})

mark_as_advanced(TBB_INCLUDE_DIR TBB_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TBB
        FOUND_VAR TBB_FOUND
        REQUIRED_VARS TBB_LIBRARY TBB_INCLUDE_DIR)
