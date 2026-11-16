#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
command -v g++ >/dev/null 2>&1 || { echo "g++ is required. Install the C++ compiler first." >&2; exit 1; }
g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  web_api.cpp "ds cp/emergency_core.cpp" "ds cp/graph_algorithms.cpp" "ds cp/emergency_system.cpp" \
  -o emergency_web
exec ./emergency_web
