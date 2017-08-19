# Find the numa policy library.
# Output variables:
#  LIBXML_INCLUDE_DIR : e.g., /usr/include/.
#  LIBXML_LIBRARY     : Library path of numa library
#  LIBXML_FOUND       : True if found.
FIND_PATH(LIBXML_INCLUDE_DIR NAME libxml
  HINTS $ENV{HOME}/local/include /opt/local/include /usr/local/include /usr/include/ /usr/include/libxml2)

FIND_LIBRARY(LIBXML_LIBRARY NAME xml2
  HINTS $ENV{HOME}/local/lib64 $ENV{HOME}/local/lib /usr/local/lib64 /usr/local/lib /opt/local/lib64 /opt/local/lib /usr/lib64 /usr/lib /usr/lib/x86_64-linux-gnu
)

IF (LIBXML_INCLUDE_DIR AND LIBXML_LIBRARY)
    SET(LIBXML_FOUND TRUE)
    MESSAGE(STATUS "Found libxml library: inc=${LIBXML_INCLUDE_DIR}, lib=${LIBXML_LIBRARY}")
ELSE ()
    SET(LIBXML_FOUND FALSE)
    MESSAGE(STATUS "WARNING: XML library not found.")
    MESSAGE(STATUS "Try: 'sudo apt-get install libxml2 libxml2-dev' (or sudo yum install libxml2 libxml2-devel)")
ENDIF ()

find_package(PackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBXML REQUIRED_VARS LIBXML_LIBRARY LIBXML_INCLUDE_DIR)
