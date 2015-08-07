SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

SET(CMAKE_C_COMPILER  i586-poky-linux-gcc)
SET(CMAKE_CXX_COMPILER i586-poky-linux-g++ -pthread)

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH /opt/poky-edison/1.7.2/sysroots/core2-32-poky-linux)

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

