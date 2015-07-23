cmake_minimum_required(VERSION 2.8)

project(sml)

set(SML_VERSION 1.0)

if (NOT CMAKE_INSTALL_LIBDIR)
  set(SML_LIB_PREFIX lib)
else()
  set(SML_LIB_PREFIX ${CMAKE_INSTALL_LIBDIR})
endif()

set(SML_LIB_DIR ${CMAKE_INSTALL_PREFIX}/${SML_LIB_PREFIX})

if (ANN_ENGINE)
  set(SML_ANN_FILES
    ${SML_INCLUDE_DIR}/sml_ann.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_ann/src/sml_ann_bridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_ann/src/sml_ann_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_ann/src/sml_ann_variable_list.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_ann/src/sml_ann_variable_list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_ann/src/sml_ann.c)
else()
  set(SML_ANN_FILES
    ${SML_INCLUDE_DIR}/sml_ann.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_ann/src/sml_ann_support.c)
endif()

if (FUZZY_ENGINE)
  set(SML_FUZZY_FILES
    ${SML_INCLUDE_DIR}/sml_fuzzy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_bit_array.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_bit_array.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_fuzzy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_fuzzy_bridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_fuzzy_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_measure.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_measure.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_observation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_observation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_observation_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_observation_controller.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_observation_group.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_observation_group.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_rule_group.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_rule_group.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_terms_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_terms_manager.c)
else()
  set(SML_FUZZY_FILES
    ${SML_INCLUDE_DIR}/sml_fuzzy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_fuzzy/src/sml_fuzzy_support.c)
endif()

if (NAIVE_ENGINE)
  set(SML_NAIVE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_naive/include/sml_naive.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_naive/src/sml_naive.c)
else()
  set(SML_NAIVE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_naive/include/sml_naive.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sml_naive/src/sml_naive_support.c)
endif()

if (MAINLOOP)
  set (SML_MAINLOOP_FILES
    ${SML_INCLUDE_DIR}/sml_main_loop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/sml_main_loop.c)
endif()

set(SML_COMMON_FILES
  ${SML_INCLUDE_DIR}/sml.h
  ${SML_INCLUDE_DIR}/sml_log.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/macros.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/sml_string.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/sml_util.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/sml_cache.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/sml_matrix.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/macros.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common/src/sml_cache.c
  ${CMAKE_CURRENT_SOURCE_DIR}/common/src/sml_matrix.c
  ${CMAKE_CURRENT_SOURCE_DIR}/common/src/sml_util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/common/src/sml_engine.c
  ${CMAKE_CURRENT_SOURCE_DIR}/common/src/sml_log.c
  ${CMAKE_CURRENT_SOURCE_DIR}/common/src/sml_string.c)

include_directories(${GLIB_INCLUDE_DIRS}
  ${SOLETTA_INCLUDE_DIRS}
  ${FANN_INCLUDE_DIRS}
  ${FUZZY_LITE_INCLUDE_DIRS}
  ${SML_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/sml_naive/include/
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

link_directories(${GLIB_LIBRARY_DIRS}
  ${SOLETTA_LIBRARY_DIRS}
  ${FANN_LIBRARY_DIRS}
  ${FUZZY_LITE_LIBRARY_DIRS})

add_library(sml_libs SHARED
  ${SML_COMMON_FILES}
  ${SML_MAINLOOP_FILES}
  ${SML_ANN_FILES}
  ${SML_FUZZY_FILES})

target_link_libraries(sml_libs
  ${SOLETTA_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${FANN_LIBRARIES}
  ${FUZZY_LITE_LIBRARIES}
  m)

add_library(naive_engine_libs SHARED
  ${SML_NAIVE_FILES})

target_link_libraries(naive_engine_libs
  sml_libs
  ${SOLETTA_LIBRARIES})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/include/config.h.in
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/config.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sml.pc.in ${CMAKE_CURRENT_SOURCE_DIR}/sml.pc @ONLY)

install(TARGETS sml_libs
  LIBRARY DESTINATION ${SML_LIB_PREFIX}
  ARCHIVE DESTINATION ${SML_LIB_PREFIX}/static)

install(DIRECTORY ${SML_INCLUDE_DIR}/ DESTINATION include)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sml.pc DESTINATION ${SML_LIB_PREFIX}/pkgconfig)
