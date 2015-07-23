cmake_minimum_required(VERSION 2.8)

project(machine-learning)

option(MAINLOOP "Build support to mainloop using glib" ON)
option(FUZZY_ENGINE "Build with fuzzy logic support" ON)
option(ANN_ENGINE "Build with neural network support" ON)
option(NAIVE_ENGINE "Build with naive engine support" ON)
option(BUILD_SOLETTA_MODULE "Build machine learning module for Soletta" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_SIMULATOR "Build simulator" ON)
option(BUILD_DOCS "Build the documentation" ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

CHECK_CXX_COMPILER_FLAG("-Wall" CXX_COMPILER_HAS_WALL)
CHECK_CXX_COMPILER_FLAG("-Wextra" CXX_COMPILER_WALL_HAS_WEXTRA)
CHECK_CXX_COMPILER_FLAG("-fvisibility=hidden" CXX_COMPILER_HAS_FVISIBILITY)

CHECK_C_COMPILER_FLAG("-std=gnu99" C_COMPILER_HAS_C99)
CHECK_C_COMPILER_FLAG("-Wall" C_COMPILER_HAS_WALL)
CHECK_C_COMPILER_FLAG("-Wextra" C_COMPILER_HAS_WEXTRA)
CHECK_C_COMPILER_FLAG("-Wno-unused-parameter" C_COMPILER_HAS_WNO_UNUSED_PARAMETER)
CHECK_C_COMPILER_FLAG("-Wno-missing-field-initializers" C_COMPILER_HAS_WNO_MISSING_FIELD_INITIALIZERS)
CHECK_C_COMPILER_FLAG("-fvisibility=hidden" C_COMPILER_HAS_FVISIBILITY)

if (CXX_COMPILER_HAS_WALL)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

if (CXX_COMPILER_WALL_HAS_WEXTRA)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
endif()

if (CXX_COMPILER_HAS_FVISIBILITY)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

if (C_COMPILER_HAS_FVISIBILITY)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
endif()

if (C_COMPILER_HAS_WEXTRA)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
endif()

if (C_COMPILER_HAS_WALL)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

if (C_COMPILER_HAS_C99)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
else()
  message(FATAL_ERROR "No gnu99 support. Bye.")
endif()

if (C_COMPILER_HAS_WNO_UNUSED_PARAMETER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
endif()

if (C_COMPILER_HAS_WNO_MISSING_FIELD_INITIALIZERS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers")
endif()

if (NOT ANN_ENGINE AND NOT FUZZY_ENGINE AND NOT NAIVE_ENGINE)
  message(FATAL_ERROR "At least on engine must be enabled in order to build SML.")
endif()

message("Fuzzy support:" ${FUZZY_ENGINE})
message("ANN support:" ${ANN_ENGINE})
message("Naive support: " ${NAIVE_ENGINE})
message("Soletta module:" ${BUILD_SOLETTA_MODULE})

set(SML_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sml/include)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SOLETTA REQUIRED soletta)

if (ANN_ENGINE)
  pkg_check_modules(FANN REQUIRED fann)
endif()

if (FUZZY_ENGINE)
  pkg_check_modules(FUZZY_LITE REQUIRED fuzzylite)
endif()

if (MAINLOOP)
  pkg_check_modules(GLIB REQUIRED glib-2.0)
endif()

add_subdirectory(sml)

if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if (BUILD_SIMULATOR)
  add_subdirectory(simulator)
endif()

if (BUILD_SOLETTA_MODULE)
  add_subdirectory(soletta_module)
endif()

if (BUILD_DOCS)
  add_subdirectory(docs)
endif()
