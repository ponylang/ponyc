cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(ponyc-driver-tests LANGUAGES C)

add_executable(ponyc-driver-stub stub.c)
add_executable(ponyc-driver-launcher launcher.c)
