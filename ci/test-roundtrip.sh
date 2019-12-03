#!/usr/bin/env bash

set -euxo pipefail

git rev-parse --git-dir 1>/dev/null # exit if not inside Git repo
readonly git_root_dir="$(git rev-parse --show-toplevel)"

readonly tsc="${git_root_dir}/build/bin/tsc"
if [[ ! -x "${tsc}" ]]; then exit 1; fi

readonly sam_file="${git_root_dir}/data/samspec.sam"
readonly tsc_file="${git_root_dir}/data/samspec.tsc"
readonly recon_file="${git_root_dir}/data/samspec.recon"

"${tsc}" "${sam_file}" --output "${tsc_file}"

"${tsc}" --decompress "${tsc_file}" --output "${recon_file}"

diff "${sam_file}" "${recon_file}"

rm "${tsc_file}"
rm "${recon_file}"
