# Native (host) unit tests for the Dictionary library, using Google Test.
#
# Local usage:
#   cd tests && cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
#
# Mirrors the TaskScheduler test harness: a mock Arduino.h (this directory) lets
# the library compile off-device, and each concern is its own test executable so
# a failure points straight at the area. Config-specific behavior (CRC width,
# packing, length types, compression) is covered by rebuilding a suite under the
# relevant -D define set.
cmake_minimum_required(VERSION 3.10)
project(DictionaryTests CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

set(DICT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SRC_DIR   ${DICT_ROOT}/src)

# BufferStream is the only always-compiled implementation unit (Dictionary.h is
# header-only in test/Arduino-IDE mode).
file(GLOB BUFFERSTREAM_SOURCES "${SRC_DIR}/BufferStream/*.cpp")

# Optional ASan/UBSan build: cmake -DDICT_SANITIZE=ON ...
option(DICT_SANITIZE "Build tests with AddressSanitizer + UBSan" OFF)

# add_dict_test(<name> SOURCE <file> [DEFINES ...] [EXTRA_SOURCES ...] [WRAP_MALLOC])
function(add_dict_test name)
    cmake_parse_arguments(T "WRAP_MALLOC" "SOURCE" "DEFINES;EXTRA_SOURCES" ${ARGN})

    add_executable(${name} ${T_SOURCE} ${BUFFERSTREAM_SOURCES} ${T_EXTRA_SOURCES})
    target_include_directories(${name} PRIVATE ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE GTest::gtest GTest::gtest_main Threads::Threads)
    target_compile_options(${name} PRIVATE -Wall -Wextra -O2)
    if(T_DEFINES)
        target_compile_definitions(${name} PRIVATE ${T_DEFINES})
    endif()
    if(T_WRAP_MALLOC)
        target_link_options(${name} PRIVATE -Wl,--wrap=malloc)
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

# ---- default-configuration suites -------------------------------------------
add_dict_test(dict_basic      SOURCE test-dictionary-basic.cpp)
add_dict_test(dict_json       SOURCE test-dictionary-json.cpp)
add_dict_test(dict_delete     SOURCE test-dictionary-delete.cpp)
add_dict_test(dict_oom        SOURCE test-dictionary-oom.cpp   WRAP_MALLOC)

# ---- configuration variants (reuse the basic suite under other defines) -----
add_dict_test(dict_crc16      SOURCE test-dictionary-basic.cpp DEFINES _DICT_CRC=16)
add_dict_test(dict_crc64      SOURCE test-dictionary-basic.cpp DEFINES _DICT_CRC=64)
add_dict_test(dict_packed     SOURCE test-dictionary-basic.cpp DEFINES _DICT_PACK_STRUCTURES)
add_dict_test(dict_longlen    SOURCE test-dictionary-basic.cpp DEFINES _DICT_KEYLEN=300 _DICT_VALLEN=1000)

# ---- compression suites -----------------------------------------------------
add_dict_test(dict_smaz  SOURCE test-dictionary-compress.cpp
              DEFINES _DICT_COMPRESS_SMAZ  EXTRA_SOURCES ${SRC_DIR}/smaz/smaz.c)
add_dict_test(dict_shoco SOURCE test-dictionary-compress.cpp
              DEFINES _DICT_COMPRESS_SHOCO EXTRA_SOURCES ${SRC_DIR}/shoco/shoco.c)
target_include_directories(dict_shoco PRIVATE ${SRC_DIR}/shoco)
