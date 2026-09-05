#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 1 || $# -gt 2 || ${1:-} == --help ]]; then
    echo "Usage: $0 BOTTLE_NAME [PROGRAM_NAME]"
    echo "Example: $0 CapCut CapCut"
    exit 0
fi
bottle_name=$1
program_name=${2:-CapCut}
log_dir=${CAPCUT_LOG_DIR:-${XDG_STATE_HOME:-$HOME/.local/state}/capcut-linux}
mkdir -p -- "$log_dir"
log_file="$log_dir/capcut-$(date +%Y%m%d-%H%M%S)-$$.log"
echo "Logging to $log_file"
flatpak run --command=bottles-cli com.usebottles.bottles run \
    -b "$bottle_name" -p "$program_name" 2>&1 | tee "$log_file"
