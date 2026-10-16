# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(LinkConsumer LANGUAGES C)

find_package(Link 0.12 CONFIG REQUIRED)

add_executable(link-package-consumer main.c)
target_compile_features(link-package-consumer PRIVATE c_std_11)
target_link_libraries(link-package-consumer PRIVATE Link::Core)
