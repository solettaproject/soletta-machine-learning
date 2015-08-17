SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

SET(CMAKE_C_COMPILER  i586-poky-linux-uclibc-gcc)
SET(CMAKE_CXX_COMPILER i586-poky-linux-uclibc-g++ -pthread)

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH $ENV{PKG_CONFIG_SYSROOT_DIR})

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

