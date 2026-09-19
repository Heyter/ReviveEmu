# ReviveEmu

ReviveEmu is a cross-platform fork of the classic RevEmu Steam.dll emulator. This fork also contains the Linux legacy `libsteamclient.so` backend used by **Counter-Strike: Source V34 / Build 4100**.

For `server_v34`, ReviveEmu is the **single supported emulator/authentication backend**. Do not stack eSTEAMATiON, REVOLUTiON, or another Steam emulator on top of it.

Russian documentation: [README_RU.md](README_RU.md).

## Production build / GitLab CI

`.gitlab-ci.yml` builds the production package only on the `prod` branch.

The GitLab Runner:

1. installs the i386/multilib toolchain;
2. verifies the pinned Build 4100 inputs in `bin-deps/SHA256SUMS`;
3. builds `libsteam.so`, the legacy `libsteamclient.so` backend, and the validator as ELF32/i386;
4. runs CTest;
5. adds the required runtime dynamic libraries from `bin-deps`;
6. lays out the package exactly like the `server_v34` runtime tree;
7. publishes both the ready directory and `.tar.gz` as GitLab job artifacts.

Build entry point:

```sh
./releases/build-prod.sh
```

Output:

```text
releases/
├── ReviveEmu-server_v34-prod/
│   ├── bin/
│   │   ├── libsteam.so
│   │   ├── steamclient_i486.so
│   │   ├── libsteamvalidateuseridtickets_i486.so
│   │   ├── libsteamvalidateuseridtickets_valve.so
│   │   ├── valve_api_i486.so
│   │   ├── steam_api_i486.so
│   │   └── rev.ini
│   ├── rev.ini
│   ├── cleanup-old-emulators.sh
│   ├── REVIVEEMU_README_RU.md
│   ├── BUILD_INFO.txt
│   └── SHA256SUMS
└── ReviveEmu-server_v34-prod.tar.gz
```

The tarball intentionally has **no outer wrapper directory**, so it can be extracted directly into the server root:

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
```

The `bin-deps/` directory itself is build input only and is not shipped. Runtime dynamic libraries from it (`*.so`, `*.so.*`, `*.dll`, `*.dylib`) are copied directly to `server_v34/bin/`.

## `bin-deps`: Build 4100 binary dependencies

`ReviveEmu/bin-deps/` contains the pinned binary inputs from the accepted `server_v34` Build 4100 baseline:

```text
bin-deps/
├── libsteamvalidateuseridtickets_valve.so
├── valve_api_i486.so
├── SHA256SUMS
└── README.md
```

- `libsteamvalidateuseridtickets_valve.so` is the original Valve Build 4100 validator preserved under a separate name for the legacy `BSL::*` ABI.
- `valve_api_i486.so` is the clean original Valve Steam API shim.
- `SHA256SUMS` pins the accepted files and is checked by CI before every production build.

The old `server_v34/bin/steamclient_i486.so` is not included because the compiled ReviveEmu backend replaces it. The legacy `server_v34/bin/steam_api_i486.so` is also not used because the supplied legacy server copy contains eSTEAMATiON. The production builder creates a clean `bin/steam_api_i486.so` from `bin-deps/valve_api_i486.so`.

### Local production build

Debian/Ubuntu:

```sh
apt-get update
apt-get install -y build-essential cmake ninja-build file git binutils patchelf gcc-multilib g++-multilib
./releases/build-prod.sh
```

## Installing and operating on `server_v34`

### 1. Extract the release

Stop the server and extract the tarball directly into the `server_v34` root:

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
cd /path/to/server_v34
```

No manual library renaming is required; the production builder already uses the exact Build 4100 runtime filenames.

### 2. Remove conflicting legacy emulators

After extraction run:

```sh
chmod +x cleanup-old-emulators.sh
./cleanup-old-emulators.sh
```

The script removes conflicting legacy files/configuration such as:

```text
bin/steamclient.so
bin/libeST_SCI.so
bin/libeST_STEAM2.so
bin/libSteam2Auth.so
rev.cfg
*esteamation* configs
*revolution* configs
```

It does **not** remove active ReviveEmu files such as `bin/steamclient_i486.so`, `bin/libsteamvalidateuseridtickets_i486.so`, `bin/libsteam.so`, or `rev.ini`.

### 3. Verify the release

```sh
sha256sum -c SHA256SUMS
```


### 4. Configure `rev.ini`

The release already ships with the recommended server configuration:

```ini
[Emulator]
CacheEnabled=False
Language=English
Logging=True
SteamUser=ReviveServer
CompatibilityMode=None
ForceRevClient=False

[Log]
FileSystem=False
Account=False
UserID=True
```

Do **not** add `SteamDll=` for CS:S V34 Build 4100.

The package contains `rev.ini` both in the server root and in `bin/`. If you edit it, keep both copies identical:

```sh
cp rev.ini bin/rev.ini
```

### 5. Start the server

ReviveEmu is **not a separate daemon/process**. There is no ReviveEmu executable to launch. SRCDS loads the libraries from `bin/` itself.

Start `server_v34` using its normal launch path, such as the existing `./start.sh`, service unit, or container entrypoint used by your deployment.

Recommended environment for the Linux legacy SteamClient backend:

```sh
REVIVE_STEAMCLIENT_LOG=./revive_steamclient.log
REVIVE_LOG_MODE=production
REVIVE_ABI_TRACE=0
```

`Logging=True` in `rev.ini` controls the classic `rev.log`; `REVIVE_LOG_MODE` controls the separate Linux SteamClient backend log.

### Updating

1. stop the server;
2. preserve your customized `rev.ini` if necessary;
3. extract the new archive over `server_v34`;
4. run `./cleanup-old-emulators.sh`;
5. run `sha256sum -c SHA256SUMS`;
6. restore/edit your `rev.ini` and run `cp rev.ini bin/rev.ini`;
7. start the server normally.

## `rev.ini` reference

ReviveEmu loads `rev.ini` from the ReviveEmu library directory first. If it is not there, it tries the process/current working directory.

### `[Emulator]`

| Key | Values / default | Purpose |
| --- | --- | --- |
| `CacheEnabled` | `True` / `False`, default `False` | Enables the old Steam GCF cache filesystem. For an extracted/loose-file CS:S V34 server keep it `False`. |
| `CachePath` | filesystem path | Directory containing `.gcf` files. Used only when `CacheEnabled=True`. If the path is empty, GCF support is disabled even if `CacheEnabled=True`. |
| `CDRPath` | file path; default is `cdr.bin` next to `rev.ini` | Optional raw Content Description Record blob used to describe applications/cache requirements. Relevant only to GCF mounting. |
| `Language` | e.g. `English`, `French`, `Italian`, `German`, `Spanish`, `sChinese`, `Korean`, `Koreana`, `tChinese`, `Japanese`, `Russian`, `Thai`, `Portuguese`; default `English` | Selects the legacy Steam language used for localized content/cache requirements. On Windows, an explicit INI value takes priority over the old Steam registry language. |
| `Logging` | `True` / `False`, default `False` | Enables the classic ReviveEmu `rev.log`. The `[Log]` switches below select extra logging groups. |
| `SteamDll` | path to an official legacy `Steam.dll`; unset by default | Loads/forwards to an official Steam client DLL for legitimate Steam UserID validation in legacy configurations. **Must remain unset for this project's CS:S V34 Build 4100 server.** |
| `SteamUser` | string, default `RevUser` | Emulated legacy Steam username. ReviveEmu also exports it through the `SteamUser` environment variable. For the dedicated server `ReviveServer` is a clear value. |
| `CompatibilityMode` | `None` or `2003`, default `None` | Enables old Steam.dll behavior required by some very old GoldSource-era builds. CS:S V34 uses `None`. |
| `ForceRevClient` | `True` / `False`, default effectively `False` | When `True`, unknown/non-Revive client tickets are rejected instead of being accepted with an IP-derived identity. This is a compatibility switch for the classic validator path. The current `server_v34` config keeps it `False`; strict ClassicRevEmu ticket handling for the Linux legacy SteamClient backend is implemented separately in that backend. |
| `SteamClient` | `True` / `False`, default `False`; Windows only | Enables loading an adjacent `steamclient.dll` by publishing legacy Steam registry values. It is not used by the Linux `server_v34` deployment. |

### `[Log]`

These switches only have an effect when `[Emulator] Logging=True`.

| Key | Purpose |
| --- | --- |
| `FileSystem` | Adds verbose logging for the legacy filesystem/GCF operations. Useful for cache/mount debugging; normally `False` on a loose-file server. |
| `Account` | Adds logging for legacy account/subscription API calls. Normally unnecessary for a dedicated CS:S server. |
| `UserID` | Adds logging for Steam UserID/ticket validation calls. This is the most useful classic validator log category when diagnosing old server authentication. |

### Recommended logging during troubleshooting

For a short diagnostic session:

```ini
[Emulator]
Logging=True

[Log]
FileSystem=False
Account=False
UserID=True
```

For normal operation, keep only the logging needed for the deployment. `rev.log` and `revive_steamclient.log` are separate files and can be managed independently.

## Generic usage

### Loose files

For a legacy game that directly uses Steam.dll:

1. Put the matching ReviveEmu Steam library next to the game executable (for Source Engine, usually in `bin/`).
2. Put `rev.ini` next to the ReviveEmu library or in the game working directory.
3. Configure the app ID using `steam_appid.txt`, `-appid <id>`, or the `SteamAppId` environment variable when the game requires it.
4. Start the game/server.

### GCF mounting

1. Set `CacheEnabled=True`.
2. Set `CachePath` to the directory containing the GCF files.
3. Provide one application/cache description source: `cdr.bin`, `ClientRegistry.blob`, or a configured `revApps.ini`.
4. Set the correct Steam app ID.
5. Start the game/server.

## Important `server_v34` rules

- ReviveEmu is the only emulator backend.
- Keep the original `valve_api_i486.so`.
- Restore `steam_api_i486.so` from `valve_api_i486.so` before deployment.
- Preserve the original Valve validator as `libsteamvalidateuseridtickets_valve.so`.
- Install ReviveEmu `libsteamclient.so` as `bin/steamclient_i486.so`.
- Install the patched ReviveEmu validator as `bin/libsteamvalidateuseridtickets_i486.so`.
- Do not keep `bin/steamclient.so`, eSTEAMATiON libraries, REVOLUTiON binaries/configuration, or another Steam emulator in the same runtime.
- Do not set `SteamDll=`.
- Keep `CacheEnabled=False` unless you deliberately switch the server to GCF mounting.

## Credits

- shmelle, revCrew, vityan666, bir3yk — original 2008 RevEmu.
- Bestest — Linux testing.
- Pancakes — macOS testing.
