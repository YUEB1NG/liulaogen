cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(MINIMAL_BUILD ON)
set(PROJECT_VER "cast-wifi-20261008-r5")
project(FoloToy-AI-Passport)
