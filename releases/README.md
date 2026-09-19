# Production releases

`releases/build-prod.sh` is the canonical **Linux x86/i386-only** production build for CS:S V34 / Build 4100. x64 packages are not built.

## GitLab Runner -> GitLab Releases

A push to `prod` runs two jobs:

1. `reviveemu:prod:server-v34` builds, tests, validates, and packages the runtime.
2. `reviveemu:release:server-v34` creates/updates the GitLab Release and uploads the finished files to the project's Generic Package Registry.

The production download location is **GitLab -> Deploy -> Releases**. Job artifacts are retained for 30 days only as CI diagnostics; release assets are stored separately in the Generic Package Registry.

No GitHub credentials are required. The release job uses GitLab's built-in `CI_JOB_TOKEN` through `glab` CI auto-login.

Automatic tags use:

```text
0.0.<CI_PIPELINE_IID>
```

Release assets:

```text
ReviveEmu-server_v34-prod.tar.gz
ReviveEmu-server_v34-prod.tar.gz.sha256
```

The files are published under the Generic Package Registry package name:

```text
reviveemu-server-v34
```

`glab release create` is intentionally used instead of the legacy `release-cli`. If a Release with the same tag already exists, `glab` updates it instead of failing immediately.

## Output layout

The tarball has **no `bin-deps/` directory**. It is already laid out as an overlay for the root of `server_v34`:

```text
ReviveEmu-server_v34-prod/
├── bin/
│   ├── libsteam.so
│   ├── steamclient_i486.so
│   ├── libsteamvalidateuseridtickets_i486.so
│   ├── libsteamvalidateuseridtickets_valve.so
│   ├── valve_api_i486.so
│   ├── steam_api_i486.so
│   └── rev.ini
├── rev.ini
├── cleanup-old-emulators.sh
├── REVIVEEMU_README_RU.md
├── BUILD_INFO.txt
└── SHA256SUMS
```

`bin-deps` is build input only. Linux runtime files matching `*.so` / `*.so.*` are copied directly into the release `bin/` directory.

Install directly into the server root:

```sh
tar -xzf ReviveEmu-server_v34-prod.tar.gz -C /path/to/server_v34
cd /path/to/server_v34
./cleanup-old-emulators.sh
sha256sum -c SHA256SUMS
```

ReviveEmu is loaded by SRCDS; there is no separate daemon to start.

## `bin-deps`

The current Build 4100 compatibility inputs provide:

- `bin/libsteamvalidateuseridtickets_valve.so`
- `bin/valve_api_i486.so`

The builder creates `bin/steam_api_i486.so` from the clean Valve shim and installs compiled ReviveEmu outputs as:

- `libsteam.so` -> `bin/libsteam.so`
- `libsteamclient.so` -> `bin/steamclient_i486.so`
- `libsteamvalidateuseridtickets.so` -> `bin/libsteamvalidateuseridtickets_i486.so`

The active validator receives `DT_NEEDED=libsteamvalidateuseridtickets_valve.so` and `RPATH=$ORIGIN` before packaging.

## Local x86 production build

Debian/Ubuntu requirements:

```sh
apt-get update
apt-get install -y build-essential cmake ninja-build file git binutils patchelf gcc-multilib g++-multilib
./releases/build-prod.sh
```

Optional build variables:

- `REVIVE_BUILD_DIR`
- `REVIVE_RELEASE_DIR`
- `REVIVE_RELEASE_NAME`
- `REVIVE_LEGACY_MILESTONE`
- `REVIVE_BUILD_PARALLEL`

The local build creates both:

```text
releases/ReviveEmu-server_v34-prod.tar.gz
releases/ReviveEmu-server_v34-prod.tar.gz.sha256
```
