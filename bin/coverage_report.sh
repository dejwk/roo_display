#!/usr/bin/env bash
set -euo pipefail

# Generates Bazel coverage and renders an HTML report (in bazel-out/coverage_html)
# Usage:
#   ./bin/coverage_report.sh                  # coverage for //:all
#   ./bin/coverage_report.sh //:driver_ili9341_test

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROO_DISPLAY_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TARGET="${1:-//:all}"

if ! command -v bazel >/dev/null 2>&1; then
  echo "error: bazel not found in PATH" >&2
  exit 1
fi

if ! command -v genhtml >/dev/null 2>&1; then
  echo "error: genhtml not found in PATH (install lcov package)" >&2
  exit 1
fi

cd "${ROO_DISPLAY_DIR}"

echo "==> Running Bazel coverage for target: ${TARGET}"
bazel coverage "${TARGET}" \
  --combined_report=lcov \
  --instrumentation_filter='^//' \
  --nocache_test_results

REPORT="$(bazel info output_path)/_coverage/_coverage_report.dat"
OUT_DIR="$(bazel info output_path)/coverage_html"

if [[ ! -f "${REPORT}" ]]; then
  echo "error: coverage report not found: ${REPORT}" >&2
  exit 1
fi

if [[ ! -s "${REPORT}" ]]; then
  echo "error: coverage report is empty: ${REPORT}" >&2
  exit 1
fi

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}"

echo "==> Generating HTML report in: ${OUT_DIR}"
# Some toolchain combinations emit consistency warnings in tracefiles.
genhtml "${REPORT}" \
  --ignore-errors inconsistent,corrupt,unsupported \
  --output-directory "${OUT_DIR}"

echo "==> Coverage HTML report ready: ${OUT_DIR}/index.html"
