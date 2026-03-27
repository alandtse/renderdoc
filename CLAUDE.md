# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Fork Notice

This is an unofficial community fork of [baldurk/renderdoc](https://github.com/baldurk/renderdoc) that permits appropriate LLM-assisted development. All other upstream contribution guidelines apply. Fork-specific issues must be filed here, not upstream.

**Branch structure:**
- `dev` — fork's main branch; all PRs target this
- `synced_debuggers` — feature branch (synchronized pixel shader debugging)
- `upstream/v1.x` — tracked directly; `dev` is periodically merged from it via an automated daily workflow

**Fork feature tracking:** [`FORK_FEATURES.md`](../FORK_FEATURES.md) must be kept up to date. When adding, changing, or removing any fork-specific feature, update that file in the same commit. It is the canonical record of what diverges from upstream.

**Licensing:** Upstream files remain MIT (`LICENSE.md`). Fork-specific contributions are GPL-3.0-or-later (`COPYING`). Contributors must read `CONTRIBUTOR_LICENSE_AGREEMENT.md` before submitting — it includes a conditional MIT relicensing right for upstream reintegration with explicit risk acknowledgment.

## Fork Identification in Builds

`renderdoc/api/replay/version.h` has `DISTRIBUTION_NAME`, `DISTRIBUTION_VERSION`,
and `DISTRIBUTION_CONTACT` defines designed for exactly this purpose. The CI sets
these via CMake flags to identify fork builds in the About dialog:

```
-DDISTRIBUTION_NAME="alandtse/renderdoc"
-DDISTRIBUTION_VERSION="<tag or dev>"
-DDISTRIBUTION_CONTACT="https://github.com/alandtse/renderdoc/releases"
```

The auto-update mechanism calls `renderdoc.org/getupdateurl` (Baldur's server) which
we do not use. Since `RENDERDOC_OFFICIAL_BUILD` is always 0 for our builds, users
clicking "Check for Updates" are shown a manual dialog — the three "view builds" links
in `qrenderdoc/Windows/MainWindow.cpp` are redirected to our GitHub releases page.
Do not replace the full update API; that divergence is not worth the maintenance cost.

## Building

### Windows

Open `renderdoc.sln` in Visual Studio 2015 or later. Use the `Development` configuration for day-to-day work, `Release` for final builds. No external dependencies — everything needed is in the checkout.

For command-line builds without touching any project files:

```bat
util\buildscripts\build-windows.cmd [Development|Release] [x64|x86] [--no-extras]
```

This detects VS automatically via vswhere and passes `/p:PlatformToolset` and `/p:WindowsTargetPlatformVersion` as MSBuild overrides, leaving all `.vcxproj` files untouched.

**To verify a build compiles after making changes, always use this script** — do not open the solution in the IDE as that will retarget the `.vcxproj` files. Use `--no-extras` to skip the 3rdparty UI asset download when only checking compilation:

```bat
util\buildscripts\build-windows.cmd Development x64 --no-extras
```

**Do not commit `.vcxproj` retargeting changes.** Opening the solution in VS2022 will modify ~24 `.vcxproj` files (toolset `v140→v143`, SDK version). These are local IDE noise — the build script and CI both override the toolset without modifying files. Run `git restore -- '*.vcxproj'` to discard them.

### Linux

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -Bbuild -H.
make -C build
```

Requires gcc 5+ or clang 3.4+ (C++14). Use `CC`/`CXX` env vars to override the compiler. CMake options can disable specific backends (e.g. `-DENABLE_GL=OFF`).

### Android

```bash
mkdir build-android && cd build-android
cmake -DBUILD_ANDROID=On -DANDROID_ABI=armeabi-v7a ..
make
```

Requires `JAVA_HOME`, `ANDROID_SDK_ROOT`, and `ANDROID_NDK_ROOT` set. Must run from bash (not cmd.exe on Windows).

## Code Formatting

Formatting is CI-enforced. The version is fixed at **clang-format 15.0.7**. The binary is in the repo at `util/clangformat/clang-format-15.0.exe`.

```bash
# Format a single file
util/clangformat/clang-format-15.0.exe -i file.cpp

# Format the entire codebase
bash util/clang_format_all.sh
```

Formatting fixes must be squashed into the relevant commit, not left as a separate commit.

**Pre-commit hook** enforces clang-format automatically on `git commit`. To install:

```bash
pip install pre-commit
pre-commit install
```

## Running Tests

```bash
# Build test demos (Linux/macOS)
cmake -Bbuild -Hdemos && make -C build

# Run tests
python util/test/run_tests.py [options]
#   --renderdoc <path>    path to renderdoc binary
#   --pyrenderdoc <path>  path to pyrenderdoc module
#   -t / --test_include   regex of tests to include
#   -x / --test_exclude   regex of tests to exclude
```

Test output lands in `artifacts/` and is browser-viewable.

## Coding Style

- Use `NULL`, not `nullptr`
- Use `rdcarray` instead of `std::vector`; use `rdcstr` instead of `std::string`
- `auto` only for STL iterators and lambdas — use explicit types everywhere else
- If any branch of an if/else needs braces, all branches get braces
- `m_` prefix for member variables; no other Hungarian notation
- STL use is limited to `std::map`, `std::set`, `std::function`, algorithms, and type traits
- `#pragma once` belongs only in `.h` files, never in `.cpp`
- Use `RDCASSERT`/`RDCERR`/`RDCWARN` for assertions and logging in core code — not `assert()` or `printf`
- Qt string formatting: use `QFormatStr("%1 %2").arg(a).arg(b)` for dynamic strings; use `lit("text")` for string literals
- New public interfaces added to `QRDInterface.h` require `DOCUMENT()` macros for Python binding generation — follow the existing pattern in that file
- New UI panels must implement `ICaptureViewer` and call `m_Ctx.AddCaptureViewer(this)` in the constructor and `m_Ctx.RemoveCaptureViewer(this)` in the destructor

## Code Quality

- **DRY**: reuse existing functions and utilities before writing new ones — check `QRDUtils.cpp`, `QRDInterface.h`, and the relevant driver files first
- **Minimal changes**: make the smallest correct change; avoid reformatting surrounding code, renaming things out of scope, or restructuring working logic
- **No dead code**: do not commit unused variables, unreachable branches, commented-out code, or stubs
- **Comments**: write comments that explain *why*, not *what*; keep them concise; do not describe prior code state unless the note is specifically warning against a regression

## Commit Guidelines

- Subject line: 72 characters max, then a blank line, then optional body
- No merge commits in PRs — use `git rebase` against `dev` to stay current
- Formatting/compile-fix commits must be squashed into the commit they fix
- Keep PRs under ~1000 lines; split large features into incremental chunks
- `dev` stays in sync with upstream via daily automated merge from `upstream/v1.x` (`.github/workflows/upstream-sync.yml`); conflicts open a GitHub issue for manual resolution

## Architecture

RenderDoc is split into two major layers:

**Core runtime (`renderdoc/`)** — a C++ shared library loaded into the target process. It hooks graphics API calls, captures frame data, and can replay captures.

- `driver/` — one subdirectory per graphics API (`vulkan/`, `d3d11/`, `d3d12/`, `gl/`, `metal/`), plus `ihv/` for vendor-specific extensions (AMD, ARM, Intel, NV) and `shaders/` for DXBC/DXIL/SPIRV handling
- `core/` — central dispatch and plugin management
- `replay/` — capture replay and analysis engine
- `serialise/` — serialization/deserialization of capture data
- `os/` — platform-specific abstractions (posix/, win32/, apple/)
- `hooks/` — API function hooking mechanisms

**UI (`qrenderdoc/`)** — a Qt5 application that loads the core as a library and presents the replay interface.

- `Windows/` — individual tool windows (shader viewer, texture viewer, pipeline state, etc.)
- `Code/` — application logic, capture context, Python bindings
- `Styles/` — custom Qt styles and theming
- `3rdparty/` — bundled UI dependencies (toolwindowmanager for docking, etc.)

**Other components:**

- `renderdoccmd/` — command-line utility wrapping the core library
- `renderdocshim/` — minimal Windows DLL for global hooking (depends only on kernel32)
- `util/test/` — Python test runner and self-contained API demo programs

The core library exposes a stable C API (`renderdoc_app.h`) and a richer C++ replay API (`replay/` headers). The UI exclusively uses the C++ replay API via the `ICaptureContext` interface.
