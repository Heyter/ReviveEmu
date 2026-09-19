# Production releases

`releases/build-prod.sh` is the canonical **Linux x86/i386-only** production build for CS:S V34 / Build 4100. x64 packages are not built.

## GitHub Actions -> GitHub Releases

`.github/workflows/build.yml` runs the production build on pushes to `prod`. Pull requests run the same build/test/package validation without publishing a Release.

For a successful `prod` run the workflow:

1. builds and tests the Linux x86/i386 binaries;
2. creates `ReviveEmu-server_v34-prod.tar.gz` and its `.sha256` file;
3. stores the output as a 30-day GitHub Actions artifact;
4. creates/reuses tag `0.0.<GITHUB_RUN_NUMBER>` for the exact workflow commit;
5. creates or updates the GitHub Release for that tag;
6. uploads the archive and checksum;
7. verifies through the GitHub API that the Release and both assets exist.

The release step uses the automatically provided `GITHUB_TOKEN`. The workflow declares `contents: write`; a custom PAT or repository secret is not required under normal GitHub Actions repository policy.

Release assets:

```text
ReviveEmu-server_v34-prod.tar.gz
ReviveEmu-server_v34-prod.tar.gz.sha256
```

The publisher is retry-safe. Re-running the same workflow run reuses the same tag, updates the release metadata, replaces matching assets, and verifies the final state.

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
