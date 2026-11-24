#!/usr/bin/env bash
# mayhem/test.sh — runs upstream's whole host-platform test suite (runtime/cmake/PlatformTests.cmake,
# configured by mayhem/build.sh the way upstream's Linux CI does, into build-mayhem/platform) with
# ctest, counting every ctest test in CTRF, plus the known-answer oracle (its decoded output must match
# mayhem/oracle/expected.txt exactly). Never compiles: a missing build fails loudly.
#
# Upstream tests NOT registered in this image, and why:
#   mkw_pad_trigger_tests       needs the SDL3::SDL3 target (SDL3 is not in the base image; upstream
#                               only registers it when SDL3 is found).
#   gx_* / mkw_gx_dl_cache_tests need the aurora GX tree (gx_fifo_tests target, xxHash, SDL3), which
#                               only exists in the full product build.
#   mkw_*_windows_*/host_file_recovery/windows_host_context  Windows-only (MinGW).
#   mkw_*_v3_tests              registered only when the build host supports x86-64-v3 (upstream's
#                               own configure-time check); on such hosts they run and are counted.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

P="$SRC/build-mayhem/platform"
passed=0 failed=0
if [ ! -f "$P/CTestTestfile.cmake" ]; then
  echo "FAIL platform tests: $P not configured (build.sh did not build it)"; failed=$((failed+1))
else
  out=$(ctest --test-dir "$P" --output-on-failure --no-tests=error 2>&1); rc=$?
  echo "$out"
  # One line per test: " N/M Test #N: name ....   Passed 0.01 sec" or "***Failed" / "Not Run" / ...
  p=$(grep -cE '^ *[0-9]+/[0-9]+ Test +#[0-9]+: .* Passed ' <<<"$out")
  t=$(grep -cE '^ *[0-9]+/[0-9]+ Test +#[0-9]+: ' <<<"$out")
  passed=$((passed + p)); failed=$((failed + t - p))
  if [ "$rc" -ne 0 ] && [ "$t" -eq "$p" ]; then echo "FAIL ctest rc=$rc"; failed=$((failed+1)); fi
  # Tests upstream registers unconditionally on Linux x86_64: each must have run (and so be counted).
  for name in mkw_ppc_cr_logical_tests mkw_platform_paths_tests mkw_runtime_config_tests \
      mkw_memory_access_tests mkw_nand_save_tests mkw_nand_guest_path_tests mkw_host_file_tests \
      mkw_system_bridge_tests mkw_u8_archive_tests mkw_runtime_config_io_tests mkw_ax_mix_kernels_tests \
      mkw_fiber_manager_tests mkw_audio_backend_ownership_tests mkw_network_receive_tests \
      mkw_windows_seh_order_tests mkw_nand_settings_tests mkw_sc_serial_tests mkw_input_expr_tests \
      mkw_motion_trick_tests mkw_bluetooth_controller_tests mkw_linux_host_context_tests \
      mkw_ppc_pair_fma_v2_tests mkw_psq_helpers_tests mkw_psq_reserved_tests mkw_psq_gqr_state_free_tests; do
    grep -qE "Test +#[0-9]+: $name " <<<"$out" || { echo "FAIL $name: not registered/run"; failed=$((failed+1)); }
  done
fi

T="$SRC/build-mayhem/tests"
if [ -x "$T/known_answers" ] && diff -u "$SRC/mayhem/oracle/expected.txt" <("$T/known_answers" "$SRC/mayhem" 2>/dev/null); then
  echo "PASS known_answers"; passed=$((passed+1))
else
  echo "FAIL known_answers (output differs from mayhem/oracle/expected.txt)"; failed=$((failed+1))
fi

emit_ctrf "cmake-ctest" "$passed" "$failed" 0
