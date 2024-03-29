# ==> Ensure proper CMake support

cmake_minimum_required(VERSION 3.10)

# ==> Force new C++ standard (we like new features here)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Odnokursniki VERSION 1.0)

# find_library(LIBUV_LIBRARY REQUIRED NAMES uv libuv)
# find_path   (LIBUV_INCLUDE REQUIRED NAMES "uv.h")


# ==> Build in release by default (makes difference for graphical projects)

set(DEFAULT_BUILD_TYPE "Release")

# ==> Add option to force ANSI-colors (useful for building with Ninja)

option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)

if (${FORCE_COLORED_OUTPUT})
  if     ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-fdiagnostics-color=always)
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options (-fcolor-diagnostics)
  endif ()
endif ()

# ==> Enable optimization
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D NDEBUG -O2")

# ==> Flags for debugging, enable sanitizers and make debugging experience as good as possible
# ==> -D DBG_MODE flag enables self written stack asserts for more agressive data control

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D DEBUG_VERSION -ggdb3 -std=c++20 -Wall -Wextra -Weffc++ -Wcast-align -Wcast-qual -Wchar-subscripts -Wctor-dtor-privacy -Wempty-body -Wfloat-equal -Wformat-nonliteral -Wformat-security -Wformat=2 -Winline -Wnon-virtual-dtor -Woverloaded-virtual -Wpacked -Wpointer-arith -Wredundant-decls -Wsign-promo -Wstrict-overflow=2 -Wsuggest-override -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused -Wvariadic-macros -Wno-missing-field-initializers -Wno-narrowing -Wno-old-style-cast -Wno-varargs -fcheck-new -fsized-deallocation -fstack-check -fstack-protector -fstrict-overflow -fno-omit-frame-pointer -fPIE -march=native -mtune=native -O0")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Waggressive-loop-optimizations -Wconditionally-supported -Wformat-signedness -Wlogical-op -Wopenmp-simd -Wstrict-null-sentinel -Wsuggest-attribute=noreturn -Wsuggest-final-methods -Wsuggest-final-types -Wsync-nand -Wuseless-cast -fconcepts-diagnostics-depth=3 -Wno-literal-suffix")
endif ()

# ==> add sub modules
add_subdirectory(inc/libuv)
add_subdirectory(inc/list)
add_subdirectory(inc/messages)

# ==> Build tests

add_executable(server server.cpp src/server.cpp src/chat_base.cpp src/networking.cpp src/sender.cpp)
add_executable(client client.cpp src/client.cpp src/chat_base.cpp src/networking.cpp src/sender.cpp)

#target_link_libraries(server      PUBLIC ${LIBUV_LIBRARY} chat_msg list)
target_link_libraries(server       PUBLIC uv_a chat_msg list)
#target_include_directories(server PUBLIC ${LIBUV_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_include_directories(server       PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

#target_link_libraries(client      PUBLIC ${LIBUV_LIBRARY} chat_msg list)
target_link_libraries(client       PUBLIC uv_a chat_msg list)
#target_include_directories(client PUBLIC ${LIBUV_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_include_directories(client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)