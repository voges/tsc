#!/usr/bin/env bash

set -euxo pipefail

readonly git_root_dir="$(git rev-parse --show-toplevel)"

readonly build_dir="${git_root_dir}/build"
if [[ ! -d "${build_dir}" ]]; then
    mkdir -p "${build_dir}"
fi

cd "${build_dir}"
cmake ..
make --jobs
