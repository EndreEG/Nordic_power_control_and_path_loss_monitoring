#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

#Minimum version of CMake
cmake_minimum_required(VERSION 3.20.0)

#Where to find Zephyr and its build system files
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

#Start the CMake project. NONE is a placeholder to replace a custom name for the project
project(NONE)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
)

# NORDIC SDK APP END
zephyr_library_include_directories(.)
