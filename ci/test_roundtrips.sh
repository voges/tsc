#!/usr/bin/env bash

set -euo pipefail

readonly git_root_dir="$(git rev-parse --show-toplevel)"

readonly tsc="${git_root_dir}/build/bin/tsc"
if [[ ! -x "${tsc}" ]]; then exit 1; fi

while IFS= read -r -d '' f; do
    sam_file="${f}"
    tsc_file="${f}.tsc"
    recon_file="${f}.recon"

    echo "Testing roundtrip with: ${sam_file}"

    "${tsc}" "${sam_file}" --output "${tsc_file}" 1> /dev/null
    "${tsc}" --decompress "${tsc_file}" --output "${recon_file}" 1> /dev/null

    diff "${sam_file}" "${recon_file}"

    rm "${tsc_file}"
    rm "${recon_file}"
done <   <(find "${git_root_dir}/data/" -type f -name "*.sam" -print0)
