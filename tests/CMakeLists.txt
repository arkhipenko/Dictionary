# Native (host) unit tests for the Dictionary library, using Google Test.
#
# Local usage:
#   cd tests && cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
#
# A mock Arduino.h (this directory) lets the library compile off-device, and each
# concern is its own test executable so a failure points straight at the area.
# Configuration-specific behavior is covered by rebuilding a suite under the
# relevant -D define set.
cmake_minimum_required(VERSION 3.10)
project(DictionaryTests CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

set(DICT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SRC_DIR   ${DICT_ROOT}/src)

# Optional ASan/UBSan build: cmake -DDICT_SANITIZE=ON ...
option(DICT_SANITIZE "Build tests with AddressSanitizer + UBSan" OFF)

# add_dict_test(<name> SOURCE <file> [DEFINES ...] [EXTRA_SOURCES ...] [WRAP_MALLOC] [QUIET_WARNINGS] [NO_GTEST])
function(add_dict_test name)
    cmake_parse_arguments(T "WRAP_MALLOC;QUIET_WARNINGS;NO_GTEST" "SOURCE" "DEFINES;EXTRA_SOURCES" ${ARGN})

    add_executable(${name} ${T_SOURCE} ${T_EXTRA_SOURCES})
    target_include_directories(${name} PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    if(T_NO_GTEST)
        # own main(); the exit code is the result
        target_link_libraries(${name} PRIVATE Threads::Threads)
    else()
        target_link_libraries(${name} PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)
    endif()
    target_compile_options(${name} PRIVATE -Wall -Wextra -O2)
    if(T_DEFINES)
        target_compile_definitions(${name} PRIVATE ${T_DEFINES})
    endif()
    if(T_QUIET_WARNINGS)
        # legacy-flag builds: the #warning about removed 3.x flags is expected
        target_compile_options(${name} PRIVATE -Wno-cpp)
    endif()
    if(T_WRAP_MALLOC)
        # _Znwm is the mangled global operator new(size_t) (64-bit Itanium ABI),
        # wrapped so allocations made with `new` are fault-injected as well.
        target_link_options(${name} PRIVATE -Wl,--wrap=malloc,--wrap=_Znwm)
        # Keep malloc/free out of the builtin optimizer so --wrap reliably
        # intercepts the (inlined) allocations for fault injection.
        target_compile_options(${name} PRIVATE -fno-builtin-malloc -fno-builtin-free)
    endif()
    if(DICT_SANITIZE)
        target_compile_options(${name} PRIVATE -fsanitize=address,undefined -g)
        target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ---- 3.x API compatibility (deprecated wrappers must keep 3.x behavior) ------
set(COMPAT _DICT_NO_DEPRECATION_WARNINGS)
add_dict_test(dict_basic        SOURCE test-dictionary-basic.cpp   DEFINES ${COMPAT})
add_dict_test(dict_json         SOURCE test-dictionary-json.cpp    DEFINES ${COMPAT})
add_dict_test(dict_delete       SOURCE test-dictionary-delete.cpp  DEFINES ${COMPAT})
add_dict_test(dict_longlen      SOURCE test-dictionary-basic.cpp   DEFINES ${COMPAT} _DICT_KEYLEN=300 _DICT_VALLEN=1000)
add_dict_test(dict_legacy_flags SOURCE test-dictionary-basic.cpp   DEFINES ${COMPAT} _DICT_CRC=16 _DICT_PACK_STRUCTURES QUIET_WARNINGS)

# ---- 4.0 API ------------------------------------------------------------------
add_dict_test(dict_api          SOURCE test-dictionary-api.cpp)
add_dict_test(dict_api_strict   SOURCE test-dictionary-api.cpp     DEFINES _DICT_STRICT_GET)
add_dict_test(dict_api_wide     SOURCE test-dictionary-api.cpp     DEFINES _DICT_WIDE_NUMBERS)
add_dict_test(dict_api_compress SOURCE test-dictionary-api.cpp     DEFINES _DICT_COMPRESS)

# ---- jload parser ---------------------------------------------------------------
add_dict_test(dict_jload        SOURCE test-dictionary-jload.cpp)
add_dict_test(dict_jload_typed  SOURCE test-dictionary-jload.cpp   DEFINES _DICT_TYPED_JSON)
add_dict_test(dict_jload_ascii  SOURCE test-dictionary-jload.cpp   DEFINES _DICT_ASCII_ONLY)
add_dict_test(dict_jload_bigbuf SOURCE test-dictionary-jload.cpp   DEFINES _DICT_VALLEN=1000)

# ---- differential test against a reference model ----------------------------------
add_dict_test(dict_oracle          SOURCE test-dictionary-oracle.cpp)
add_dict_test(dict_oracle_noindex  SOURCE test-dictionary-oracle.cpp DEFINES _DICT_INDEX_MIN=0)
add_dict_test(dict_oracle_index1   SOURCE test-dictionary-oracle.cpp DEFINES _DICT_INDEX_MIN=1)
add_dict_test(dict_oracle_compress SOURCE test-dictionary-oracle.cpp DEFINES _DICT_COMPRESS)

# ---- value codec -------------------------------------------------------------------
add_dict_test(dict_compress        SOURCE test-dictionary-compress.cpp DEFINES _DICT_COMPRESS)
add_dict_test(dict_compress_legacy SOURCE test-dictionary-compress.cpp DEFINES _DICT_COMPRESS_SMAZ QUIET_WARNINGS)

# ---- out-of-memory fault injection ---------------------------------------------------
add_dict_test(dict_oom          SOURCE test-dictionary-oom.cpp WRAP_MALLOC)
add_dict_test(dict_oom_compress SOURCE test-dictionary-oom.cpp WRAP_MALLOC DEFINES _DICT_COMPRESS)

# ---- PlatformIO split-header mode ------------------------------------------------------
# The test source sees only DictionaryDeclarations.h; the implementation comes
# from the bundled src/Dictionary.cpp, compiled as its own translation unit.
add_dict_test(dict_split SOURCE test-dictionary-split.cpp
              DEFINES _DICT_HEADER_AND_CPP EXTRA_SOURCES ${SRC_DIR}/Dictionary.cpp)

# ---- example sketch: Dict_Benchmark ----------------------------------------------------
# The sketch is compiled against the shim and run once; it checks its own results and
# the exit code is the number of failed checks.
add_dict_test(dict_bench_example          SOURCE bench-example-host.cpp NO_GTEST DEFINES BENCH_PAIRS=10000)
add_dict_test(dict_bench_example_compress SOURCE bench-example-host.cpp NO_GTEST DEFINES BENCH_PAIRS=2000 _DICT_COMPRESS)
add_dict_test(dict_bench_example_noindex  SOURCE bench-example-host.cpp NO_GTEST DEFINES BENCH_PAIRS=1000 _DICT_INDEX_MIN=0)

# ---- compile-only checks: deprecation warnings --------------------------------------
set(COMPILE_CHECK ${CMAKE_CXX_COMPILER} -std=c++14 -fsyntax-only -Werror=deprecated-declarations
    -I${SRC_DIR} -I${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME dict_deprecation_warns
         COMMAND ${COMPILE_CHECK} ${CMAKE_CURRENT_SOURCE_DIR}/compile-deprecated.cpp)
set_tests_properties(dict_deprecation_warns PROPERTIES WILL_FAIL TRUE)
add_test(NAME dict_deprecation_silenced
         COMMAND ${COMPILE_CHECK} -D_DICT_NO_DEPRECATION_WARNINGS ${CMAKE_CURRENT_SOURCE_DIR}/compile-deprecated.cpp)
add_test(NAME dict_new_api_no_warnings
         COMMAND ${COMPILE_CHECK} ${CMAKE_CURRENT_SOURCE_DIR}/compile-new-api.cpp)
