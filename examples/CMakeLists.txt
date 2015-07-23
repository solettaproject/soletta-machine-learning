cmake_minimum_required(VERSION 2.8)

project(examples)

include_directories(${SML_INCLUDE_DIR} ${GLIB_INCLUDE_DIRS} ${FUZZY_LITE_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS} ${FUZZY_LITE_LIBRARY_DIRS} ${FANN_LIBRARY_DIRS} ${SOLETTA_LIBRARY_DIRS})

message("SML include dir:" ${SML_INCLUDE_DIR})

if (FUZZY_ENGINE)
    add_executable(fuzzy_test.out fuzzy_test.cpp)
    target_link_libraries(fuzzy_test.out ${GLIB_LIBRARIES} sml_libs)
endif()

if (MAINLOOP)
    add_executable(example_air_conditioner.out example_air_conditioner.c)
    target_link_libraries(example_air_conditioner.out ${GLIB_LIBRARIES} sml_libs m)
endif()

add_executable(example_sensor_file.out example_sensor_file)
add_executable(test_memory.out test_memory)
add_executable(foosball_example.out foosball_example)
add_executable(example_doc.out example_doc.c)
target_link_libraries(example_sensor_file.out ${GLIB_LIBRARIES} sml_libs)
target_link_libraries(test_memory.out ${GLIB_LIBRARIES} sml_libs)
target_link_libraries(foosball_example.out ${GLIB_LIBRARIES} sml_libs)
target_link_libraries(example_doc.out sml_libs)
