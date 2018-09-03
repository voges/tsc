#!/bin/bash

if [ "$#" -ne 0 ]; then
    printf "usage: $0\n"
    exit -1
fi

script_name="$(basename $0)"

source_dir="../source"
if [ ! -d $source_dir ]; then
    printf "$script_name: error: folder '$source_dir' is not a directory\n"
    exit -1
fi

cpplint_cfg="../source/CPPLINT.cfg"
if [ ! -f $cpplint_cfg ]; then
    printf "$script_name: error: file '$cpplint_cfg' is not a regular file\n"
    exit -1
fi

python="/usr/bin/python"
if [ ! -x $python ]; then
    printf "$script_name: error: binary file '$python' is not executable\n"
    exit -1
fi

cpplint_py="cpplint.py"
if [ ! -f $cpplint_py ]; then
    printf "$script_name: error: Python script '$cpplint_py' is not a regular file\n"
    exit -1
fi

files=()
files+=("$source_dir/tsc.cc")
# to be continued

for file in "${files[@]}"; do
    printf "$script_name: running cpplint on: $file\n"

    if [ ! -f $file ]; then
        printf "$script_name: error: file '$file' is not a regular file\n"
        exit -1
    fi

    $python $cpplint_py $file
done
