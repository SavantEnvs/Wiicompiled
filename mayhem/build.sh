#!/usr/bin/env bash
# mayhem/build.sh — builds the WiiCompiled runtime's self-contained parsers as libFuzzer targets
# (+ -standalone reproducers), and upstream's full host-platform test suite + the known-answer
# oracle that mayhem/test.sh runs. The full runtime cannot be built (it links C++ translated from the user's
# own game disc), so this compiles the same source/include sets upstream's
# runtime/cmake/PlatformTests.cmake uses for each unit, directly with $CXX.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
R="$SRC/runtime"
B="$SRC/build-mayhem"          # gitignored by upstream's /build-*/
mkdir -p "$B/fuzz" "$B/tests"

INC=(-I"$R/include" -I"$R/third_party/pugixml" -I"$R/third_party/toml11")
# -fsanitize=fuzzer-no-link: SanCov edge instrumentation on the project AND harness objects.
FUZZ_CXXFLAGS=(-std=c++20 -O1 -fsanitize=fuzzer-no-link $SANITIZER_FLAGS $DEBUG_FLAGS "${INC[@]}")

# Project sources each target needs (compiled with the sanitizers, like the harness).
declare -A SRCS=(
  [fuzz_riivolution]="$R/third_party/pugixml/pugixml.cpp"
  [fuzz_runtime_config]=""
  [fuzz_u8_archive]=""
  [fuzz_input_expr]="$R/src/input_expr.cpp"
  [fuzz_dolphin_ini]="$R/src/input_expr.cpp"
  [fuzz_wiimote_ext]="$R/src/wiimote/wiimote_extension.cpp $R/src/wiimote/wiimote_ext_crypto.cpp"
  [fuzz_bluetooth]="$R/src/hle/bluetooth/bt_controller.cpp $R/src/hle/bluetooth/bt_remote_link.cpp $R/src/hle/bluetooth/bt_sdp.cpp"
)

# Shared objects: each project source once, the LSan-off hook and the standalone driver.
objs_for() {
  local out=() s o
  for s in ${SRCS[$1]}; do
    o="$B/fuzz/$(basename "${s%.cpp}").o"
    out+=("$o")
  done
  echo "${out[@]:-}"
}
pids=()
for s in $(printf '%s\n' ${SRCS[@]} | sort -u); do
  "$CXX" "${FUZZ_CXXFLAGS[@]}" -c "$s" -o "$B/fuzz/$(basename "${s%.cpp}").o" & pids+=($!)
done
"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/fuzz/lsan_off.o" & pids+=($!)
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/fuzz/standalone_main.o" & pids+=($!)
for p in "${pids[@]}"; do wait "$p"; done

pids=()
for t in "${!SRCS[@]}"; do
  (
    "$CXX" "${FUZZ_CXXFLAGS[@]}" -c "$SRC/mayhem/harnesses/$t.cpp" -o "$B/fuzz/$t.o"
    # shellcheck disable=SC2046
    "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/fuzz/$t.o" $(objs_for "$t") \
      "$B/fuzz/lsan_off.o" -pthread -o "$SRC/$t"
    # shellcheck disable=SC2046
    "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS "$B/fuzz/$t.o" $(objs_for "$t") "$B/fuzz/standalone_main.o" \
      "$B/fuzz/lsan_off.o" -pthread -o "$SRC/$t-standalone"
  ) & pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done

# Upstream's whole host-platform test suite, configured exactly as upstream's Linux CI does
# (.github/workflows/build.yml: -DMKW_BUILD_PRODUCTS=OFF -DMKW_PLATFORM_TESTS_ONLY=ON, Release), with
# NORMAL flags (no sanitizers) into build-mayhem/platform; mayhem/test.sh runs it with ctest. Two
# deliberate differences, both widening or stabilising the suite:
#   -DMKW_BUILD_PSQ_TESTS=ON   also builds upstream's opt-in PSQ ISA tests (psq_helpers,
#                              psq_reserved, psq_gqr_state_free).
#   -DMKW_X86_CPU_PROFILE=v2   compiles the profile-dependent tests (ppc_cr_logical, PSQ) for
#                              x86-64-v2 so they also run on a grader host without AVX2/FMA; the
#                              explicit *_v3 tests are still registered whenever the host supports v3.
# CMake takes $CC/$CXX (never -DCMAKE_*_COMPILER: a re-run would drop the cache). Tests-only mode
# returns before the product build's FetchContent, so this configures and builds offline.
P="$B/platform"
cmake -S "$R" -B "$P" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DMKW_BUILD_PRODUCTS=OFF -DMKW_PLATFORM_TESTS_ONLY=ON -DMKW_BUILD_PSQ_TESTS=ON -DMKW_X86_CPU_PROFILE=v2 \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build "$P" --parallel "$MAYHEM_JOBS"

# The known-answer oracle (mayhem/oracle/known_answers.cpp) over the fuzzed parsers, NORMAL flags.
TEST_FLAGS=(-std=c++20 -O2 $COVERAGE_FLAGS "${INC[@]}")
"$CXX" "${TEST_FLAGS[@]}" "$SRC/mayhem/oracle/known_answers.cpp" "$R/src/input_expr.cpp" \
  "$R/third_party/pugixml/pugixml.cpp" -pthread -o "$B/tests/known_answers"
echo "build.sh: done"
