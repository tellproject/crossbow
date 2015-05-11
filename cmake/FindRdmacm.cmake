# - Find Rdmacm

# Look for the rdmacm header
find_path(Rdmacm_INCLUDE_DIR
        NAMES rdma/rdma_cma.h
        HINTS ${Rdmacm_ROOT} ENV Rdmacm_ROOT
        PATH_SUFFIXES include)

# Look for the rdmacm library
find_library(Rdmacm_LIBRARY
        NAMES rdmacm
        HINTS ${Rdmacm_ROOT} ENV Rdmacm_ROOT
        PATH_SUFFIXES lib)

set(Rdmacm_LIBRARIES ${Rdmacm_LIBRARY})
set(Rdmacm_INCLUDE_DIRS ${Rdmacm_INCLUDE_DIR})

mark_as_advanced(Rdmacm_INCLUDE_DIR Rdmacm_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Rdmacm
        FOUND_VAR Rdmacm_FOUND
        REQUIRED_VARS Rdmacm_LIBRARY Rdmacm_INCLUDE_DIR)
