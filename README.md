# cyan-cpp

[![Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml)

A native C++20 rewrite of [cyan](https://github.com/asdfzxcvbn/pyzule-rw) for Windows x64. It modifies authorized `.ipa`, `.tipa`, and `.app` packages without requiring Python, WSL, macOS, or Xcode at runtime.

> [!WARNING]
> Version 0.3.0 is a beta release focused on performance and efficiency. The project leaves beta at 1.0.0. Keep a copy of every input file.

## Features

- Injects `.dylib`, `.deb`, `.framework`, `.bundle`, and `.appex` content.
- Changes app metadata and applies ordered `.cyan` packages.
- Supports thin and FAT Mach-O files, arm64 thinning, entitlements, and `ldid` signing.
- Handles spaces and Unicode characters in Windows paths.
- Provides ipapatch v2.1.3 behavior as both an integrated backend and a standalone CLI.
- Uses atomic output publishing; failed operations do not replace the input.

## Download

Open [Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml), select a successful `Windows 2022 / Release` run, and download `cyan-cpp-windows-2022-x64`.

The archive contains `cyan.exe`, `cgen.exe`, `ipapatch.exe`, `ldid.exe`, the pinned ipapatch payload, and required runtime DLLs.

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

## 0.3.0 verification

Local Release tests used decrypted YouTube Music 9.29, Instagram 439.0.0, and TikTok 46.1.0 IPAs:

- Standalone ipapatch patched 5, 9, and 10 executable targets respectively.
- ZIP CRC, normalized file trees, payload placement, load commands, metadata changes, and unchanged input hashes passed.
- Standalone C++ ipapatch median total time fell from 83.39 seconds in 0.2.0 to 27.84 seconds in 0.3.0.
- Dylib plus `-uwdsq` median total time fell from 96.62 seconds to 75.19 seconds.
- Integrated Cyan plus ipapatch continued to use one full extraction and one packaging pass.

These are local corpus measurements, not universal speed guarantees. Before publishing the GitHub Release, the clean hosted `Windows 2022 / Release` workflow must build, test, smoke-test, package, and upload the 0.3.0 artifact successfully. Device installation tests, malformed-input fuzzing, low-disk tests, and long-path tests remain recommended before 1.0.0.

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

## Credits

Technical acknowledgement goes to [pyzule-rw](https://github.com/asdfzxcvbn/pyzule-rw), [ipapatch](https://github.com/asdfzxcvbn/ipapatch), [Azule](https://github.com/mpelteshki/Azule), [LIEF](https://github.com/lief-project/LIEF), [insert_dylib](https://github.com/Tyilo/insert_dylib), [libarchive](https://github.com/libarchive/libarchive), [libzip](https://github.com/nih-at/libzip), and [Procursus ldid](https://github.com/ProcursusTeam/ldid).
