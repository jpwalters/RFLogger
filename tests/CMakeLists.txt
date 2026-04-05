set(TEST_SOURCES
    tst_SweepData.cpp
    tst_ScanSession.cpp
    tst_WwbExporter.cpp
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name}
        ${test_src}
        ${CMAKE_SOURCE_DIR}/src/data/SweepData.cpp
        ${CMAKE_SOURCE_DIR}/src/data/ScanSession.cpp
        ${CMAKE_SOURCE_DIR}/src/export/WwbExporter.cpp
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
    )
    target_link_libraries(${test_name} PRIVATE
        Qt6::Core
        Qt6::Test
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
