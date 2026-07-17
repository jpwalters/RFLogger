set(TEST_SOURCES
    tst_SweepData.cpp
    tst_ScanSession.cpp
    tst_WwbExporter.cpp
    tst_GenericCsvExporter.cpp
    tst_SessionSerializer.cpp
    tst_SignalClassifier.cpp
    tst_InterferenceMonitor.cpp
)

# Shared production sources exercised by the tests. Compile them exactly once
# into a static library instead of recompiling them in every test target — this
# cuts the number of compile jobs (and peak build memory) dramatically.
add_library(rflogger_testable STATIC
    ${CMAKE_SOURCE_DIR}/src/data/SweepData.cpp
    ${CMAKE_SOURCE_DIR}/src/data/ScanSession.cpp
    ${CMAKE_SOURCE_DIR}/src/data/SessionSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/data/TvChannelMap.cpp
    ${CMAKE_SOURCE_DIR}/src/data/RfBandPlan.cpp
    ${CMAKE_SOURCE_DIR}/src/data/SignalClassifier.cpp
    ${CMAKE_SOURCE_DIR}/src/data/InterferenceMonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/export/WwbExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/export/GenericCsvExporter.cpp
)
target_include_directories(rflogger_testable PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(rflogger_testable PUBLIC
    Qt6::Core
    Qt6::Gui
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE
        rflogger_testable
        Qt6::Test
    )

    if(WIN32)
        # Ensure Qt test runtime DLLs are available when CTest launches binaries directly.
        add_custom_command(TARGET ${test_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt6::Core>
                $<TARGET_FILE:Qt6::Gui>
                $<TARGET_FILE:Qt6::Test>
                $<TARGET_FILE_DIR:${test_name}>
        )
    endif()

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
