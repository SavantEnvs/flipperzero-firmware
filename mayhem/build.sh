#!/usr/bin/env bash
#
# mayhem/build.sh — builds the Flipper Zero firmware libraries that are plain C on the host
# (infrared codecs, Flipper File Format, mJS, FatFs, 125 kHz RFID protocols) together with
# the libFuzzer harnesses in mayhem/harnesses and the known-answer test mayhem/kat/kat.c.
#
# The firmware proper cross-compiles with fbt/scons for ARM, so this host build compiles the
# same library sources against a small furi shim (mayhem/shim) that shadows the FreeRTOS-backed
# headers; the one third-party header set it needs (mlib) comes from the image cache.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
COV=-fsanitize=fuzzer-no-link   # SanCov on the library code, not just the harness
: "${SRC:=/mayhem}"
export COV SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

MLIB=/opt/toolchains/deps/mlib-62c8ac3e/src
[ -f "$MLIB/m-string.h" ] || { echo "missing cached dependency: $MLIB" >&2; exit 1; }

B="$SRC/build/mayhem"
rm -rf "$B"
mkdir -p "$B/san" "$B/norm"

# Host build of firmware code: furi shim first, firmware defines, mlib allocation failure aborts.
INC=(-I. -Imayhem/shim -Ifuri -Ilib -Ilib/lfrfid -Ilib/toolbox -Ilib/mjs -Ilib/fatfs
     -Ilib/infrared/encoder_decoder -Iapplications/services -Itargets/f7/furi_hal
     -Itargets/furi_hal_include -Itargets/f7/fatfs -I"$MLIB")
DEFS=(-D_GNU_SOURCE -DNDEBUG -DFURI_NDEBUG '-DM_MEMORY_FULL(x)=abort()'
      '-D_ATTRIBUTE(x)=__attribute__(x)' -include mayhem/shim/compat.h -w)

furi_core=(furi/core/string.c mayhem/shim/furi_shim.c mayhem/shim/stubs_storage.c)
ir_src=("${furi_core[@]}" lib/infrared/encoder_decoder/infrared.c lib/infrared/encoder_decoder/*/*.c)
ff_src=("${furi_core[@]}" lib/flipper_format/flipper_format.c lib/flipper_format/flipper_format_stream.c
        lib/toolbox/stream/stream.c lib/toolbox/stream/string_stream.c lib/toolbox/stream/stream_cache.c
        lib/toolbox/hex.c lib/toolbox/strint.c)
mjs_src=("${furi_core[@]}" lib/mjs/*.c lib/mjs/common/*.c lib/mjs/common/frozen/*.c lib/mjs/ffi/*.c)
fat_src=(lib/fatfs/ff.c lib/fatfs/ff_gen_drv.c lib/fatfs/diskio.c lib/fatfs/option/unicode.c)
lf_src=("${furi_core[@]}" lib/lfrfid/protocols/*.c lib/lfrfid/tools/fsk_demod.c lib/lfrfid/tools/fsk_ocs.c
        lib/lfrfid/tools/varint_pair.c lib/toolbox/protocols/protocol_dict.c
        lib/toolbox/manchester_decoder.c lib/toolbox/manchester_encoder.c lib/toolbox/bit_buffer.c
        lib/bit_lib/bit_lib.c lib/toolbox/varint.c lib/toolbox/hex.c)

declare -A target_src=(
  [fuzz_infrared]="${ir_src[*]}"
  [fuzz_flipper_format]="${ff_src[*]}"
  [fuzz_mjs]="${mjs_src[*]}"
  [fuzz_fatfs]="${fat_src[*]}"
  [fuzz_lfrfid]="${lf_src[*]}"
)
targets=(fuzz_infrared fuzz_flipper_format fuzz_mjs fuzz_fatfs fuzz_lfrfid)

# Object path for a source file in a flavour (san|norm).
obj_of() { echo "$B/$1/$(echo "$2" | tr / _).o"; }

# Compile one source. Four UBSan checks flag idioms that are well-defined for the firmware's compiler
# and core (arm-none-eabi GCC, Cortex-M4) but would halt every run on a trivial input, so they are
# off for all firmware sources; ASan and the remaining UBSan checks stay halting:
#   function         calls through casted function pointers (protocol, stream and mJS vtables)
#   alignment        unaligned loads in the IR and RFID decoders
#   shift-base       shifting a byte into bit 31 (Samsung IR address, NexWatch)
#   pointer-overflow NULL + 0 in the mJS garbage collector
#   float-cast-overflow  double -> long for out-of-range number literals in mJS (saturates on ARM)
compile_one() {
  local flavour="$1" f="$2" extra=(-fno-sanitize=function,alignment,shift-base,pointer-overflow,float-cast-overflow)
  if [ "$flavour" = san ]; then
    $CC -O1 $SANITIZER_FLAGS $COV $DEBUG_FLAGS "${extra[@]}" "${INC[@]}" "${DEFS[@]}" -c "$f" -o "$(obj_of san "$f")"
  else
    $CC -O2 $COVERAGE_FLAGS "${INC[@]}" "${DEFS[@]}" -c "$f" -o "$(obj_of norm "$f")"
  fi
}
export -f compile_one obj_of
export B

# xargs children are fresh bash processes: pass arrays through a wrapper script instead of exports.
compile_all() {
  local flavour="$1"; shift
  printf '%s\n' "$@" | sort -u | xargs -P "$MAYHEM_JOBS" -I{} \
    bash -c 'set -euo pipefail; INC=('"${INC[*]@Q}"'); DEFS=('"${DEFS[*]@Q}"'); compile_one "$0" "$1"' "$flavour" {}
}

all_src=()
for t in "${targets[@]}"; do read -ra s <<< "${target_src[$t]}"; all_src+=("${s[@]}"); done

compile_all san "${all_src[@]}"

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"

for t in "${targets[@]}"; do
  read -ra s <<< "${target_src[$t]}"
  objs=(); for f in "${s[@]}"; do objs+=("$(obj_of san "$f")"); done
  $CC -O1 $SANITIZER_FLAGS $COV $DEBUG_FLAGS "${INC[@]}" "${DEFS[@]}" -c "mayhem/harnesses/$t.c" -o "$B/$t.o"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$t.o" "${objs[@]}" "$B/lsan_off.o" -lm -o "$SRC/$t"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$B/$t.o" "${objs[@]}" "$B/lsan_off.o" "$B/standalone_main.o" -lm -o "$SRC/$t-standalone"
done

# Known-answer test, normal flags (no sanitizers), union of every library.
kat_src=("${all_src[@]}")
compile_all norm "${kat_src[@]}"
$CC -O2 $COVERAGE_FLAGS "${INC[@]}" "${DEFS[@]}" -c mayhem/kat/kat.c -o "$B/kat.o"
kat_objs=(); while read -r f; do kat_objs+=("$(obj_of norm "$f")"); done < <(printf '%s\n' "${kat_src[@]}" | sort -u)
$CC -O2 $COVERAGE_FLAGS "$B/kat.o" "${kat_objs[@]}" -lm -o "$SRC/kat"
