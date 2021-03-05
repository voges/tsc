#!/usr/bin/env bash

git rev-parse --git-dir 1>/dev/null || exit 1
readonly git_root_dir="$(git rev-parse --show-toplevel)"
readonly build_dir="${git_root_dir}/build"
"${build_dir}/bin/rans_test"
"${build_dir}/bin/sam_test"
"${build_dir}/bin/zlib_wrapper_test"
