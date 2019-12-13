cmake_minimum_required(VERSION 3.10)

# set the project name
project(libfab-test)

include_directories(include)
set(CMAKE_BUILD_TYPE Debug)

# add the executable
add_executable(libfab-test
	src/main.c
	src/network.c
	src/server.c
	src/client.c
	src/task.c
	src/mem.c
	include/network.h
	include/log.h
	include/task.h
	include/mem.h
)
# add_subdirectory(src)
target_link_libraries(libfab-test fabric)
