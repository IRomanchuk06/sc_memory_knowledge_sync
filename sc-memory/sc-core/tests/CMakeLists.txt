find_glib()

# ============= UNIT TESTS (GoogleTest) =============

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/common
    NAME sc-core-common-tests
    DEPENDS ${glib_LIBRARIES} sc-memory
    INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-common-tests)
endif()

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/containers
    NAME sc-core-containers-tests
    DEPENDS ${glib_LIBRARIES} sc-memory
    INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-containers-tests)
endif()

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/fs-storage
    NAME sc-core-fs-storage-tests
    DEPENDS ${glib_LIBRARIES} sc-memory
    INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-fs-storage-tests)
endif()

make_tests_from_folder(${CMAKE_CURRENT_SOURCE_DIR}/units/transaction
        NAME sc-core-transaction-tests
        DEPENDS ${glib_LIBRARIES} sc-memory
        INCLUDES ${glib_INCLUDE_DIRS} ${SC_CORE_SRC}
)

if(${SC_CLANG_FORMAT_CODE})
    target_clangformat_setup(sc-core-transaction-tests)
endif()

# ============= BENCHMARKS (Google Benchmark) =============
# Создаем отдельно, чтобы избежать gtest_discover_tests

find_package(benchmark QUIET)

if(benchmark_FOUND)
    message(STATUS "Google Benchmark found, building benchmarks")
    
    # Собираем все исходники бенчмарков
    file(GLOB_RECURSE BENCHMARK_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark/*.cpp"
    )
    
    # Создаем исполняемый файл для бенчмарков
    add_executable(sc-core-transaction-benchmark-tests ${BENCHMARK_SOURCES})
    
    # Подключаем зависимости
    target_link_libraries(sc-core-transaction-benchmark-tests
        PRIVATE
        sc-core
        sc-memory
        benchmark::benchmark
        ${glib_LIBRARIES}
        pthread
    )
    
    # Линковка с --no-as-needed для корректной работы
    target_link_options(sc-core-transaction-benchmark-tests PRIVATE "-Wl,--no-as-needed")
    
    # Указываем пути для включения заголовочных файлов
    target_include_directories(sc-core-transaction-benchmark-tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
        ${glib_INCLUDE_DIRS}
        ${SC_CORE_SRC}
    )
    
    # Добавляем в CTest без gtest_discover_tests
    if(BUILD_TESTING)
        add_test(
            NAME sc-core-transaction-benchmarks
            COMMAND sc-core-transaction-benchmark-tests
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
        
        # Маркируем как бенчмарк для отдельного запуска
        set_tests_properties(sc-core-transaction-benchmarks PROPERTIES
            LABELS "benchmark;performance"
            TIMEOUT 1200
        )
    endif()
    
    # Clang-format для бенчмарков
    if(${SC_CLANG_FORMAT_CODE})
        target_clangformat_setup(sc-core-transaction-benchmark-tests)
    endif()
    
    message(STATUS "Benchmark target created: sc-core-transaction-benchmark-tests")
else()
    message(WARNING "Google Benchmark not found, benchmarks will not be built")
endif()
