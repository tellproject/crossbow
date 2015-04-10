# - Find Ibverbs
FIND_PATH(Ibverbs_INCLUDE_DIR NAMES infiniband/verbs.h)
MARK_AS_ADVANCED(Ibverbs_INCLUDE_DIR)

# Look for the library.
FIND_LIBRARY(Ibverbs_LIBRARY NAMES libibverbs.so)
MARK_AS_ADVANCED(Ibverbs_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set CURL_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE("${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake")
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Ibverbs DEFAULT_MSG Ibverbs_LIBRARY Ibverbs_INCLUDE_DIR)

IF(Ibverbs_FOUND)
  SET(Ibverbs_LIBRARIES ${Ibverbs_LIBRARY})
  SET(Ibverbs_INCLUDE_DIRS ${Ibverbs_INCLUDE_DIR})
ENDIF(Ibverbs_FOUND)

