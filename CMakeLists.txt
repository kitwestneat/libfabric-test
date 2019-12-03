cmake_minimum_required(VERSION 3.10)

# set the project name
project(libfab-test)

# add the executable
add_executable(libfab-test main.c)
target_link_libraries(libfab-test fabric)
