# ReviveEmu

ReviveEmu is the Steam emulator/authentication backend used by **Counter-Strike: Source V34 / Build 4100** in this project. The supported deployment target is Linux.

**Windows is not supported.**

For `server_v34`, ReviveEmu is the **single supported emulator/authentication backend**. Do not run eSTEAMATiON, REVOLUTiON, or another Steam emulator alongside it.

Russian documentation: [README_RU.md](README_RU.md).

## Production build / GitHub Actions

`.github/workflows/build.yml` builds the production package for pushes to the `prod` branch. Pull requests still run the build/test/package validation, but do not publish a Release.

The GitHub Actions workflow:

1. installs the Linux i386/multilib toolchain;
2. verifies the pinned Build 4100 inputs in `bin-deps/SHA256SUMS`;
3. builds `libsteam.so`, `libsteamclient.so`, and `libsteamvalidateuseridtickets.so` as ELF32/i386;
4. runs CTest;
5. copies required Linux runtime `.so` files from `bin-deps`;
6. creates a ready-to-extract `server_v34` runtime layout;
7. stores the package as a 30-day Actions artifact;
8. on `prod`, creates a **GitHub Release**, uploads the `.tar.gz` plus checksum, and verifies both uploaded assets through the GitHub API.

Only Linux x86/i386 is built. x64 builds are not produced.

No custom PAT is required for normal GitHub-hosted Actions. The workflow uses the repository-provided `GITHUB_TOKEN` and explicitly requests `contents: write` for the build job.

The production workflow creates a GitHub Release tagged `0.0.<GITHUB_RUN_NUMBER>` and uploads:

```text
ReviveEmu-server_v34-prod.tar.gz
ReviveEmu-server_v34-prod.tar.gz.sha256
```

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

The tarball has no outer wrapper directory and can be extracted directly into the server root:

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
```

`bin-deps/` is build input only and is not shipped as a directory. Runtime Linux libraries from it (`*.so`, `*.so.*`) are copied directly into `server_v34/bin/`.

## `bin-deps`

The accepted Build 4100 compatibility inputs are:

```text
bin-deps/
├── libsteamvalidateuseridtickets_valve.so
├── valve_api_i486.so
├── SHA256SUMS
└── README.md
```

- `libsteamvalidateuseridtickets_valve.so` is the original Valve Build 4100 validator retained as an ABI dependency for legacy `BSL::*` symbols.
- `valve_api_i486.so` is the clean original Valve Steam API shim.
- `SHA256SUMS` pins both files and is checked before every production build.

The old `server_v34/bin/steamclient_i486.so` is not included because ReviveEmu replaces it. The old `server_v34/bin/steam_api_i486.so` is not used because that server copy contains eSTEAMATiON; the production builder creates a clean `steam_api_i486.so` from `valve_api_i486.so`.

## Local production build

Debian/Ubuntu:

```sh
apt-get update
apt-get install -y build-essential cmake ninja-build file git binutils patchelf gcc-multilib g++-multilib
./releases/build-prod.sh
```

## Deploying to `server_v34`

### 1. Stop the server and extract the release

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
cd /path/to/server_v34
```

No manual renaming is required. The release already uses the filenames expected by Build 4100.

### 2. Remove old emulator files

```sh
chmod +x cleanup-old-emulators.sh
./cleanup-old-emulators.sh
```

The script removes conflicting legacy emulator libraries/configuration and keeps the active ReviveEmu runtime.

### 3. Verify the package

```sh
sha256sum -c SHA256SUMS
```

Run this before editing `rev.ini`.

### 4. Configure `rev.ini`

Recommended production configuration:

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

The release contains `rev.ini` in the server root and in `bin/`. If you change it, keep both copies synchronized:

```sh
cp rev.ini bin/rev.ini
```

### 5. Start the server

ReviveEmu is not a separate process. SRCDS loads the libraries from `bin/`.

Start the server using its normal launcher, for example:

```sh
./start.sh
```

Recommended runtime environment for the legacy SteamClient backend:

```sh
REVIVE_STEAMCLIENT_LOG=./revive_steamclient.log
REVIVE_LOG_MODE=production
REVIVE_ABI_TRACE=0
```

## Updating ReviveEmu

1. stop the server;
2. preserve a customized `rev.ini` if necessary;
3. extract the new archive over `server_v34`;
4. run `./cleanup-old-emulators.sh`;
5. run `sha256sum -c SHA256SUMS` before changing the shipped files;
6. restore/edit `rev.ini` and copy it to `bin/rev.ini`;
7. start the server normally.

## `rev.ini` reference

ReviveEmu first looks for `rev.ini` next to its library and then in the process working directory.

### `[Emulator]`

| Key                 | Values / default                               | Purpose                                                                                                                                                                                                                        |
| ------------------- | ---------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `CacheEnabled`      | `True` / `False`, default `False`              | Enables the legacy Steam GCF cache filesystem. Keep `False` for the normal loose-file `server_v34` deployment.                                                                                                                 |
| `CachePath`         | filesystem path                                | Directory containing `.gcf` files. Used only when `CacheEnabled=True`.                                                                                                                                                         |
| `CDRPath`           | file path; default `cdr.bin` next to `rev.ini` | Optional raw Content Description Record used for old GCF/cache metadata.                                                                                                                                                       |
| `Language`          | Steam language name; default `English`         | Selects the legacy Steam language used for localized cache/content requirements.                                                                                                                                               |
| `Logging`           | `True` / `False`, default `False`              | Enables the classic `rev.log`. The `[Log]` switches below select extra log groups.                                                                                                                                             |
| `SteamUser`         | string, default `RevUser`                      | Emulated legacy Steam username. `ReviveServer` is recommended for the dedicated server.                                                                                                                                        |
| `CompatibilityMode` | `None` or `2003`, default `None`               | Enables compatibility behavior for very old engine builds. CS:S V34 uses `None`.                                                                                                                                               |
| `ForceRevClient`    | `True` / `False`, default `False`              | Rejects unknown/non-Revive tickets in the classic validator path when enabled. The current `server_v34` configuration keeps it `False`; the Linux legacy SteamClient backend performs its own ClassicRevEmu ticket validation. |

### `[Log]`

These options are effective when `Logging=True`.

| Key          | Purpose                                                                                 |
| ------------ | --------------------------------------------------------------------------------------- |
| `FileSystem` | Verbose legacy filesystem/GCF logging. Normally `False` for `server_v34`.               |
| `Account`    | Legacy account/subscription API logging. Normally unnecessary for the dedicated server. |
| `UserID`     | Steam UserID/ticket validation logging. Most useful when diagnosing authentication.     |

### Troubleshooting logging

```ini
[Emulator]
Logging=True

[Log]
FileSystem=False
Account=False
UserID=True
```

`rev.log` and `revive_steamclient.log` are separate logs.

## Important `server_v34` rules

- ReviveEmu is the only emulator backend.
- Keep the original `valve_api_i486.so`.
- Generate `steam_api_i486.so` from the clean `valve_api_i486.so`.
- Preserve the original Valve validator as `libsteamvalidateuseridtickets_valve.so`.
- Install ReviveEmu `libsteamclient.so` as `bin/steamclient_i486.so`.
- Install the patched ReviveEmu validator as `bin/libsteamvalidateuseridtickets_i486.so`.
- Do not keep eSTEAMATiON, REVOLUTiON, or another Steam emulator in the same runtime.
- Keep `CacheEnabled=False` unless the server is intentionally switched to GCF mounting.

## Credits

- shmelle, revCrew, vityan666, bir3yk — original RevEmu.
- Bestest — Linux testing.
