#!/bin/sh
set -eu

ROOT="${1:-.}"

fail() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

remove_file() {
    path="$1"
    if [ -e "$path" ] || [ -L "$path" ]; then
        rm -f -- "$path"
        printf 'removed: %s\n' "$path"
    fi
}

[ -d "$ROOT/bin" ] || fail "server bin directory not found: $ROOT/bin"
if [ ! -f "$ROOT/srcds_i486" ] && [ ! -f "$ROOT/srcds_run" ]; then
    fail "this does not look like the server_v34 root: $ROOT"
fi

STEAMCLIENT="$ROOT/bin/steamclient_i486.so"
STEAM_API="$ROOT/bin/steam_api_i486.so"

for required in \
    "$ROOT/bin/libsteam.so" \
    "$STEAMCLIENT" \
    "$ROOT/bin/libsteamvalidateuseridtickets_i486.so" \
    "$ROOT/bin/libsteamvalidateuseridtickets_valve.so" \
    "$ROOT/bin/valve_api_i486.so" \
    "$STEAM_API" \
    "$ROOT/rev.ini" \
    "$ROOT/bin/rev.ini"
do
    [ -f "$required" ] || fail "ReviveEmu release file is missing: $required"
done

grep -a -Fq 'REVive_LegacySteamClient_BuildMarker' "$STEAMCLIENT" || \
    fail "steamclient_i486.so is not the ReviveEmu production backend; extract the release before running cleanup"

if grep -a -q 'eSTEAMATiON' "$STEAM_API"; then
    fail "steam_api_i486.so still contains eSTEAMATiON; release extraction is incomplete"
fi

# Conflicting legacy emulator binaries/configs. Do not remove the active
# ReviveEmu files installed by the release (steamclient_i486.so,
# libsteamvalidateuseridtickets_i486.so, libsteam.so, rev.ini).
remove_file "$ROOT/bin/steamclient.so"
remove_file "$ROOT/bin/libeST_SCI.so"
remove_file "$ROOT/bin/libeST_STEAM2.so"
remove_file "$ROOT/bin/libSteam2Auth.so"
remove_file "$ROOT/rev.cfg"
remove_file "$ROOT/esteamation.conf"
remove_file "$ROOT/esteamation.cfg"
remove_file "$ROOT/revolution.cfg"
remove_file "$ROOT/revolution.ini"

for cfg_dir in "$ROOT/cfg" "$ROOT/cstrike/cfg"; do
    [ -d "$cfg_dir" ] || continue
    find "$cfg_dir" -maxdepth 1 -type f \
        \( -iname '*esteamation*' -o -iname '*revolution*' \) \
        -print -delete
 done

# Final conflict check.
for path in \
    "$ROOT/bin/steamclient.so" \
    "$ROOT/bin/libeST_SCI.so" \
    "$ROOT/bin/libeST_STEAM2.so" \
    "$ROOT/bin/libSteam2Auth.so"
do
    [ ! -e "$path" ] || fail "legacy emulator file is still present: $path"
done

printf '%s\n' 'ReviveEmu cleanup complete.'
printf '%s\n' 'Active backend: bin/steamclient_i486.so'
printf '%s\n' 'Active validator: bin/libsteamvalidateuseridtickets_i486.so'
