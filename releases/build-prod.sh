#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${REVIVE_BUILD_DIR:-"$ROOT_DIR/.build/reviveemu-prod-x86"}
RELEASE_DIR=${REVIVE_RELEASE_DIR:-"$ROOT_DIR/releases"}
RELEASE_NAME=${REVIVE_RELEASE_NAME:-"ReviveEmu-server_v34-prod"}
OUTPUT_DIR="$RELEASE_DIR/$RELEASE_NAME"
ARCHIVE="$RELEASE_DIR/$RELEASE_NAME.tar.gz"
PARALLEL=${REVIVE_BUILD_PARALLEL:-2}
MILESTONE=${REVIVE_LEGACY_MILESTONE:-prod}

fail() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

for cmd in cmake ctest sha256sum file nm patchelf tar grep find cp mkdir rm; do
    require_cmd "$cmd"
done

[ -f "$ROOT_DIR/bin-deps/SHA256SUMS" ] || fail "bin-deps/SHA256SUMS is missing"
[ -f "$ROOT_DIR/bin-deps/valve_api_i486.so" ] || fail "bin-deps/valve_api_i486.so is missing"
[ -f "$ROOT_DIR/bin-deps/libsteamvalidateuseridtickets_valve.so" ] || fail "bin-deps/libsteamvalidateuseridtickets_valve.so is missing"

printf '%s\n' '==> Verifying pinned server_v34 dependencies'
(
    cd "$ROOT_DIR/bin-deps"
    sha256sum -c SHA256SUMS
)

for dep in \
    "$ROOT_DIR/bin-deps/valve_api_i486.so" \
    "$ROOT_DIR/bin-deps/libsteamvalidateuseridtickets_valve.so"
do
    file "$dep" | grep -Eq 'ELF 32-bit.*Intel 80386|ELF 32-bit.*Intel i386' || \
        fail "dependency is not Linux ELF32/i386: $dep"
done

nm -D --defined-only "$ROOT_DIR/bin-deps/libsteamvalidateuseridtickets_valve.so" | grep -q 'BSL' || \
    fail 'Valve validator does not expose the expected legacy BSL ABI symbols'

if grep -a -q 'eSTEAMATiON' "$ROOT_DIR/bin-deps/valve_api_i486.so"; then
    fail 'bin-deps/valve_api_i486.so contains eSTEAMATiON markers'
fi

printf '%s\n' '==> Configuring Linux x86/i386 production build'
rm -rf "$BUILD_DIR"
cmake \
    -S "$ROOT_DIR/Steam/steam" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON \
    -DREVIVE_LEGACY_MILESTONE="$MILESTONE" \
    -DCMAKE_C_FLAGS='-m32' \
    -DCMAKE_CXX_FLAGS='-m32' \
    -DCMAKE_EXE_LINKER_FLAGS='-m32' \
    -DCMAKE_SHARED_LINKER_FLAGS='-m32'

printf '%s\n' '==> Building'
cmake --build "$BUILD_DIR" --parallel "$PARALLEL"

printf '%s\n' '==> Running tests'
ctest --test-dir "$BUILD_DIR" --output-on-failure

LIBSTEAM="$BUILD_DIR/libsteam.so"
LIBSTEAMCLIENT="$BUILD_DIR/libsteamclient.so"
VALIDATOR="$BUILD_DIR/libsteamvalidateuseridtickets.so"

for built in "$LIBSTEAM" "$LIBSTEAMCLIENT" "$VALIDATOR"; do
    [ -s "$built" ] || fail "expected build output is missing: $built"
    file "$built" | grep -Eq 'ELF 32-bit.*Intel 80386|ELF 32-bit.*Intel i386' || \
        fail "build output is not Linux ELF32/i386: $built"
done

grep -a -Fq 'REVive_LegacySteamClient_BuildMarker' "$LIBSTEAMCLIENT" || \
    fail 'libsteamclient.so is missing REVive_LegacySteamClient_BuildMarker'

for built in "$LIBSTEAM" "$LIBSTEAMCLIENT" "$VALIDATOR"; do
    if grep -a -Eq 'eSTEAMATiON|REVOLUTiON' "$built"; then
        fail "legacy emulator marker found in ReviveEmu output: $built"
    fi
done

printf '%s\n' '==> Preparing server_v34 overlay'
rm -rf "$OUTPUT_DIR" "$ARCHIVE" "$ARCHIVE.sha256"
mkdir -p "$OUTPUT_DIR/bin"

cp "$LIBSTEAM" "$OUTPUT_DIR/bin/libsteam.so"
cp "$LIBSTEAMCLIENT" "$OUTPUT_DIR/bin/steamclient_i486.so"
cp "$VALIDATOR" "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_i486.so"

# Runtime compatibility inputs from the accepted Build 4100 server.
# bin-deps itself is intentionally never copied into the release.
find "$ROOT_DIR/bin-deps" -maxdepth 1 -type f \( -name '*.so' -o -name '*.so.*' \) -exec cp '{}' "$OUTPUT_DIR/bin/" ';'

# The old server steam_api_i486.so contained another emulator. ReviveEmu ships
# the clean Valve shim under the filename expected by Build 4100.
cp "$ROOT_DIR/bin-deps/valve_api_i486.so" "$OUTPUT_DIR/bin/steam_api_i486.so"

# The ReviveEmu validator is active, but Build 4100 still needs legacy Valve
# BSL::* ABI symbols from the preserved original validator.
if ! patchelf --print-needed "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_i486.so" | \
    grep -Fxq 'libsteamvalidateuseridtickets_valve.so'; then
    patchelf --add-needed libsteamvalidateuseridtickets_valve.so \
        "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_i486.so"
fi
patchelf --set-rpath '$ORIGIN' "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_i486.so"

cp "$ROOT_DIR/releases/server_v34/rev.ini" "$OUTPUT_DIR/rev.ini"
cp "$ROOT_DIR/releases/server_v34/rev.ini" "$OUTPUT_DIR/bin/rev.ini"
cp "$ROOT_DIR/releases/server_v34/cleanup-old-emulators.sh" "$OUTPUT_DIR/cleanup-old-emulators.sh"
cp "$ROOT_DIR/releases/server_v34/REVIVEEMU_README_RU.md" "$OUTPUT_DIR/REVIVEEMU_README_RU.md"
chmod +x "$OUTPUT_DIR/cleanup-old-emulators.sh"

BUILD_COMMIT=${GITHUB_SHA:-unknown}
BUILD_PIPELINE=
if [ -z "$BUILD_PIPELINE" ] && [ -n "${GITHUB_SERVER_URL:-}" ] && [ -n "${GITHUB_REPOSITORY:-}" ] && [ -n "${GITHUB_RUN_ID:-}" ]; then
    BUILD_PIPELINE="${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
fi
[ -n "$BUILD_PIPELINE" ] || BUILD_PIPELINE=local

cat > "$OUTPUT_DIR/BUILD_INFO.txt" <<INFO
project=ReviveEmu
target=server_v34
platform=linux
architecture=x86/i386
build_type=Release
milestone=$MILESTONE
commit=$BUILD_COMMIT
pipeline=$BUILD_PIPELINE
INFO

printf '%s\n' '==> Validating packaged runtime'
for required in \
    "$OUTPUT_DIR/bin/libsteam.so" \
    "$OUTPUT_DIR/bin/steamclient_i486.so" \
    "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_i486.so" \
    "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_valve.so" \
    "$OUTPUT_DIR/bin/valve_api_i486.so" \
    "$OUTPUT_DIR/bin/steam_api_i486.so" \
    "$OUTPUT_DIR/rev.ini" \
    "$OUTPUT_DIR/bin/rev.ini" \
    "$OUTPUT_DIR/cleanup-old-emulators.sh"
do
    [ -s "$required" ] || fail "release file is missing: $required"
done

for runtime_lib in "$OUTPUT_DIR/bin/"*.so "$OUTPUT_DIR/bin/"*.so.*; do
    [ -e "$runtime_lib" ] || continue
    file "$runtime_lib" | grep -Eq 'ELF 32-bit.*Intel 80386|ELF 32-bit.*Intel i386' || \
        fail "release contains a non-x86/i386 library: $runtime_lib"
done

patchelf --print-needed "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_i486.so" | \
    grep -Fxq 'libsteamvalidateuseridtickets_valve.so' || \
    fail 'packaged validator is missing Valve ABI dependency'

[ "$(patchelf --print-rpath "$OUTPUT_DIR/bin/libsteamvalidateuseridtickets_i486.so")" = '$ORIGIN' ] || \
    fail 'packaged validator RPATH is not $ORIGIN'

[ ! -d "$OUTPUT_DIR/bin-deps" ] || fail 'bin-deps directory must not be shipped'

(
    cd "$OUTPUT_DIR"
    find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)

printf '%s\n' '==> Creating release archive'
tar -C "$OUTPUT_DIR" -czf "$ARCHIVE" .
sha256sum "$ARCHIVE" > "$ARCHIVE.sha256"

if tar -tzf "$ARCHIVE" | grep -Eq '(^|/)bin-deps(/|$)'; then
    fail 'bin-deps path leaked into the production archive'
fi

printf 'Production release ready:\n  %s\n  %s\n' "$ARCHIVE" "$ARCHIVE.sha256"
