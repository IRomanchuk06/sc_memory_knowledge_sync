find_glib()

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/common
    NAME sc-core-common-tests
    DEPENDS ${glib_LIBRARIES} sc-memory sc-core
    INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-common-tests)
endif()

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/containers
    NAME sc-core-containers-tests
    DEPENDS ${glib_LIBRARIES} sc-memory sc-core
    INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-containers-tests)
endif()

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/fs-storage
    NAME sc-core-fs-storage-tests
    DEPENDS ${glib_LIBRARIES} sc-memory sc-core
    INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-fs-storage-tests)
endif()

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/transaction
        NAME sc-core-transaction-tests
        DEPENDS ${glib_LIBRARIES} sc-memory sc-core
        INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-transaction-tests)
endif()

if(${SC_BUILD_BENCH})
    find_package(benchmark REQUIRED)
    
    file(GLOB BENCHMARK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/*.cpp)
    add_executable(sc-core-transaction-benchmark-tests ${BENCHMARK_SOURCES})
    
    target_link_libraries(sc-core-transaction-benchmark-tests
        PRIVATE
        ${glib_LIBRARIES}
        sc-memory
        sc-core
        benchmark::benchmark
    )
    
    target_include_directories(sc-core-transaction-benchmark-tests
        PRIVATE
        ${glib_INCLUDE_DIRS}
        ${SC_CORE_SRC}
    )
    
    add_test(
        NAME sc-core-transaction-benchmark-tests
        COMMAND sc-core-transaction-benchmark-tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
    )

    if(${SC_CLANG_FORMAT_CODE})
        target_clangformat_setup(sc-core-transaction-benchmark-tests)
    endif()
endif()
