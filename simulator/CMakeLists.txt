cmake_minimum_required(VERSION 2.8)

project(simulator)

include_directories(${SML_INCLUDE_DIR} ${GLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/sml/sml_naive/include/)
link_directories(${GLIB_LIBRARY_DIRS} ${FANN_LIBRARY_DIRS} ${FUZZY_LITE_LIBRARY_DIRS} ${SOLETTA_LIBRARY_DIRS})

add_executable(sml_simulator.out sml_simulator.c)
target_link_libraries(sml_simulator.out ${GLIB_LIBRARIES} sml_libs naive_engine_libs m)
