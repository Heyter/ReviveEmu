# `bin-deps` — CS:S V34 / Build 4100 runtime dependencies

This directory contains the **original Valve binary dependencies copied from the accepted `server_v34` Build 4100 tree**. They are not ReviveEmu replacements; they are compatibility inputs required to build/deploy the ReviveEmu runtime for that server.

Files:

- `valve_api_i486.so` — original Valve Steam API shim from `server_v34/bin/valve_api_i486.so`. The active `steam_api_i486.so` must be restored from this clean original instead of using the old eSTEAMATiON-patched file present in the legacy server bundle.
- `libsteamvalidateuseridtickets_valve.so` — original Valve validator copied from `server_v34/bin/libsteamvalidateuseridtickets_i486.so` and renamed. CS:S Build 4100 imports legacy `BSL::*` ABI symbols from this library. ReviveEmu remains the active validator; this file is loaded only as its ABI compatibility dependency.
- `SHA256SUMS` — checksums of both accepted Build 4100 dependency binaries.

Source hashes from the supplied `server_v34` baseline:

```text
5301b58de41461c13e067fef58ab73434122ae5a408452fdfa369ee4487d6ee5  valve_api_i486.so
8137cced76d44ee09e63f3024a92e809461928ef21ff532409de5a3c2b54b721  libsteamvalidateuseridtickets_valve.so
```

Do **not** copy these legacy server binaries into `bin-deps`:

- `steamclient_i486.so` — replaced by ReviveEmu `libsteamclient.so`.
- `steam_api_i486.so` — the supplied server copy contains eSTEAMATiON and must not be used. The production builder creates a clean `steam_api_i486.so` by copying `valve_api_i486.so`.
- eSTEAMATiON / REVOLUTiON libraries — ReviveEmu is the only supported emulator backend.

`releases/build-prod.sh` validates this directory before building and includes the required compatibility binaries in the production release.

## System runtime libraries

The server-specific files above do not replace the normal 32-bit Linux runtime. The target host/container must still provide the i386 C/C++ runtime (`libc6:i386`, `libgcc-s1:i386`, `libstdc++6:i386` on Debian/Ubuntu). These are operating-system packages, not files copied from `server_v34`, so they are intentionally not vendored into `bin-deps`.
