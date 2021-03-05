#!/usr/bin/env bash

git rev-parse --git-dir 1>/dev/null || exit 1
readonly git_root_dir="$(git rev-parse --show-toplevel)"
readonly build_dir="${git_root_dir}/build"
"${build_dir}/bin/rans_tests"
"${build_dir}/bin/tsc_tests"
"${build_dir}/bin/util_tests"
"${build_dir}/bin/zlib_wrapper_tests"
