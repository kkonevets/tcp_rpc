cmake_minimum_required(VERSION 3.13)
project(myproject)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1) # clangd

LIST(APPEND CMAKE_PREFIX_PATH /usr/local/lib/cmake/msgpack-c/)
find_package(msgpack-c)

#set(CMAKE_BUILD_TYPE Release)

add_executable(client client.c)

target_link_libraries(client msgpack-c)
