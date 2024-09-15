#!/bin/sh

output_file="compile_flags.txt"

include_flags=$(pkg-config --cflags raylib)

echo "${include_flags}" > "${output_file}"
echo "-Wall" >> "${output_file}"
echo "-Wextra" >> "${output_file}"
echo "-Wpedantic" >> "${output_file}"
