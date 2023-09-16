add_library(configs STATIC chat_configs.cpp)

target_include_directories(configs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})