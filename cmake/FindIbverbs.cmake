# - Find Ibverbs

# Look for the ibverbs header
find_path(Ibverbs_INCLUDE_DIR
        NAMES infiniband/verbs.h
        HINTS ${Ibverbs_ROOT} ENV Ibverbs_ROOT
        PATH_SUFFIXES include)

# Look for the ibverbs library
find_library(Ibverbs_LIBRARY
        NAMES ibverbs
        HINTS ${Ibverbs_ROOT} ENV Ibverbs_ROOT
        PATH_SUFFIXES lib)

set(Ibverbs_LIBRARIES ${Ibverbs_LIBRARY})
set(Ibverbs_INCLUDE_DIRS ${Ibverbs_INCLUDE_DIR})

mark_as_advanced(Ibverbs_INCLUDE_DIR Ibverbs_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ibverbs
        FOUND_VAR Ibverbs_FOUND
        REQUIRED_VARS Ibverbs_LIBRARY Ibverbs_INCLUDE_DIR)
