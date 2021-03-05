#!/usr/bin/env bash

git rev-parse --git-dir 1>/dev/null || exit 1
readonly git_root_dir="$(git rev-parse --show-toplevel)"
readonly build_dir="${git_root_dir}/build"
mkdir --parents "${build_dir}"
cmake -S "${git_root_dir}" -B "${build_dir}" -DBUILD_TESTS=ON
cd "${build_dir}" && make --jobs
