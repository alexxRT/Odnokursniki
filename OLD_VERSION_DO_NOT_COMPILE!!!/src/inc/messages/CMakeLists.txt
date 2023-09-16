add_library(chat_msg STATIC messages.cpp)

find_path(LIBUV_INCLUDE REQUIRED NAMES "uv.h")

target_link_libraries(chat_msg PUBLIC configs)
target_include_directories(chat_msg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LIBUV_INCLUDE})
