# cyan-cpp

[![Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml)

A native C++20 rewrite of [cyan](https://github.com/asdfzxcvbn/pyzule-rw) for Windows x64. It modifies authorized `.ipa`, `.tipa`, and `.app` packages without requiring Python, WSL, macOS, or Xcode at runtime.

> [!IMPORTANT]
> Version 1.0.0 is the first stable cyan-cpp release. Package modification is inherently
> destructive, so keep a copy of every input file.

## Features

- Injects `.dylib`, `.deb`, `.framework`, `.bundle`, and `.appex` content.
- Changes app metadata and applies ordered `.cyan` packages.
- Supports thin and FAT Mach-O files, arm64 thinning, entitlements, and `ldid` signing.
- Handles spaces and Unicode characters in Windows paths.
- Provides ipapatch v2.1.3 behavior as both an integrated backend and a standalone CLI.
- Includes a native Qt 6 Quick desktop application with a job composer, sequential queue,
  persistent presets, light/dark themes, and native Windows file pickers.
- Can copy GUI-only Payload Root items directly to `IPA/Payload/<name>` without treating them as
  app-bundle injections.
- Uses atomic output publishing; failed operations do not replace the input.

## Download

Download the latest stable Windows package from
[GitHub Releases](https://github.com/canpng/cyan-cpp/releases/latest). Version 1.0.0 is published as
`cyan-cpp-v1.0.0-windows-x64.zip`. Development snapshots remain available from successful
[Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml) runs.

The archive contains `cyan-gui.exe`, `cyan.exe`, `cgen.exe`, `ipapatch.exe`, `ldid.exe`, the pinned
ipapatch payload, and required runtime DLLs.

## Desktop application

Run `cyan-gui.exe` for the native Windows interface. The main workflow is:

```text
Select IPA -> choose preset/tweaks -> choose output -> add to queue -> start queue
```

Archive, signing, Mach-O, and filesystem work runs outside the QML thread. Queue entries hold an
immutable C++ snapshot and run one at a time. GUI presets are stored as Unicode JSON under the
user's application-data directory; they are intentionally different from portable `.cyan`
packages.

The **Payload Root** tab is also intentionally separate from injection. Its items are copied to
`<package>/Payload/<item-name>`, beside the `.app` bundle, and never to the application root.

## Usage

Inject a dylib and apply the common Cyan options:

```text
cyan -i input.ipa -f Tweak.dylib -uwdsq -c 9 --overwrite -o output.ipa
```

Multiple `-f` paths are separated by spaces, not commas.

| Flag | Action |
| --- | --- |
| `-u` | Remove the supported-device list |
| `-w` | Remove Watch content |
| `-d` | Enable document sharing |
| `-s` | Ad-hoc sign with `ldid` |
| `-q` | Thin Mach-O files to arm64 |
| `-e` | Remove app extensions |
| `-g` | Remove encrypted extensions |

Run `cyan --help` for every option.

### Integrated ipapatch

```text
cyan -i input.ipa -f Tweak.dylib -uwdsq --ipapatch --overwrite -o output.ipa
```

Cyan and ipapatch share one open package directory. The IPA is fully extracted and packaged only once, and all Mach-O mutations finish before final signing.

### Standalone ipapatch

```text
ipapatch --input input.ipa --output output.ipa --noconfirm
```

Useful options are `--dylib`, `--plugins-only`, and `--inplace`. Standalone mode updates only the changed ZIP entries. `--inplace` still writes a temporary result, validates it, and atomically replaces the input after success.

The main executable and eligible immediate `.appex` bundles receive a weak `@rpath/<payload>` load command. Watch content is skipped. Each executable keeps its own identifier, Team ID, signature flags, platform, and XML/DER entitlement behavior.

Entitlements are held in memory or in an isolated temporary workspace and passed directly to the final signing operation. cyan-cpp intentionally does not package a temporary `cyan.entitlements` or `cyan-cpp.entitlements` file inside the app.

The bundled `zxPluginsInject.dylib` comes from ipapatch v2.1.3:

```text
SHA-256  cd903ea15657cbd356398adcb60c8872c41c29b69acc1a5dfb78a49d6e75dea5
```

## 1.0.0 release

Version 1.0.0 promotes cyan-cpp from beta and introduces the native Qt 6 desktop application,
sequential background job queue, persistent presets and settings, light/dark themes, and the safe
Payload Root workflow. It retains the optimized archive and ipapatch pipeline verified during the
0.3.0 performance cycle.

### Performance baseline

Local Release tests used decrypted YouTube Music 9.29, Instagram 439.0.0, and TikTok 46.1.0 IPAs:

- Standalone ipapatch patched 5, 9, and 10 executable targets respectively.
- ZIP CRC, normalized file trees, payload placement, load commands, metadata changes, and unchanged input hashes passed.
- Standalone C++ ipapatch median total time fell from 83.39 seconds in 0.2.0 to 27.84 seconds in 0.3.0.
- Dylib plus `-uwdsq` median total time fell from 96.62 seconds to 75.19 seconds.
- Integrated Cyan plus ipapatch continued to use one full extraction and one packaging pass.

These are local corpus measurements, not universal speed guarantees. Stable release tags are
published only after the clean hosted Windows workflow builds, tests, packages, and verifies the
CLI and GUI deliverables. Device installation tests, malformed-input fuzzing, low-disk tests, and
long-path tests remain recommended for future hardening.

## Build

Use Developer PowerShell for Visual Studio 2022 with the x64 C++ workload and CMake 3.28 or newer:

```powershell
git clone https://github.com/microsoft/vcpkg ..\vcpkg
git -C ..\vcpkg checkout cd61e1e26a038e82d6550a3ebbe0fbbfe7da78e3
$env:VCPKG_ROOT = (Resolve-Path ..\vcpkg)

.\scripts\bootstrap-vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT
.\scripts\fetch-ldid.ps1 -OutputPath .\out\tools\ldid.exe
$ldid = (Resolve-Path .\out\tools\ldid.exe).Path

cmake --preset windows-release -DCYAN_LDID_EXECUTABLE="$ldid"
cmake --build --preset windows-release
ctest --preset windows-release
cmake --install out\build\windows-release --prefix out\package
```

The standard Windows presets enable the vcpkg `gui` feature and build Qt 6. To build only the CLI
and core libraries, configure with `-DCYAN_BUILD_GUI=OFF` and omit the `gui` manifest feature.

## Publishing a stable release

After the version commit passes Windows CI, open **Actions**, select **Release**, choose
**Run workflow**, and enter the `v2.0.0` tag. The workflow verifies that the tag matches the
project version, builds and tests the MSVC/Qt package, creates the stable GitHub Release, and
attaches the versioned Windows ZIP.

## Credits

Technical acknowledgement goes to [pyzule-rw](https://github.com/asdfzxcvbn/pyzule-rw), [ipapatch](https://github.com/asdfzxcvbn/ipapatch), [Azule](https://github.com/mpelteshki/Azule), [LIEF](https://github.com/lief-project/LIEF), [insert_dylib](https://github.com/Tyilo/insert_dylib), [libarchive](https://github.com/libarchive/libarchive), [libzip](https://github.com/nih-at/libzip), and [Procursus ldid](https://github.com/ProcursusTeam/ldid).
