# Production releases

`releases/build-prod.sh` is the canonical **Linux x86/i386-only** production build for CS:S V34 / Build 4100. x64 packages are not built.

## GitLab Runner -> GitHub Releases

A push to `prod` runs one production job: `reviveemu:prod:server-v34`. It builds, tests, validates, packages, publishes the generated `.tar.gz` and `.sha256` to **GitHub Releases**, and then verifies that the Release contains both uploaded assets.

The job cannot finish successfully if GitHub publication or verification fails. GitLab job artifacts are retained only as temporary CI diagnostics. GitHub Releases is the production download location.

### Required GitLab CI/CD variables

Configure under **GitLab -> Settings -> CI/CD -> Variables**:

- `GITHUB_TOKEN` — fine-grained GitHub token with `Contents: Read and write`. Store it as Masked/Hidden. If the variable is Protected, `prod` must also be a protected branch.
- `GITHUB_REPOSITORY` — **required** target GitHub repository in `owner/repository` format. It is intentionally not inferred from `$CI_PROJECT_PATH`.
- `GITHUB_TARGET_COMMITISH` — optional fallback GitHub branch/commit. If omitted and the exact GitLab commit is not present on GitHub, the publisher uses the GitHub repository default branch.

The publisher first checks whether `CI_COMMIT_SHA` exists on GitHub. If it does, the release tag targets that exact commit. Otherwise it uses `GITHUB_TARGET_COMMITISH` when configured, or the GitHub default branch.

Automatic tags use:

```text
0.0.<CI_PIPELINE_IID>
```

Release assets:

```text
ReviveEmu-server_v34-prod.tar.gz
ReviveEmu-server_v34-prod.tar.gz.sha256
```

The publisher is retry-safe: if the release/tag already exists for the same pipeline, it updates the release metadata, replaces matching assets, and uploads the new files.

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
