# cyan-cpp

[![Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml)

A native C++20 rewrite of [cyan](https://github.com/asdfzxcvbn/pyzule-rw) for Windows x64. It modifies authorized `.ipa`, `.tipa`, and `.app` packages without requiring Python, WSL, macOS, or Xcode at runtime.

> [!WARNING]
> This project is in preview. It is not yet verified as a drop-in replacement for cyan 1.4.4. Keep a copy of every input.

## Features

- Inject `.dylib`, `.deb`, `.framework`, `.bundle`, and `.appex` content.
- Apply ordered `.cyan` packages.
- Edit metadata, icons, entitlements, extensions, and Watch content.
- Inspect and rebuild thin or FAT Mach-O binaries.
- Thin binaries to arm64 and preserve XML entitlements.
- Ad-hoc sign with the packaged Procursus `ldid.exe`.
- Handle spaces and Unicode characters in Windows paths.

## Download

Open [Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml), choose a successful run, and download `cyan-cpp-windows-2022-x64`. Stable packages will appear under [Releases](https://github.com/canpng/cyan-cpp/releases) after the release checklist below is complete.

## Quick start

Open PowerShell or Command Prompt in the extracted folder:

```text
cyan -i input.ipa -f Tweak.dylib -o output.ipa
```

Multiple `-f` inputs do not need commas. Short flags can be grouped:

```text
cyan -i "C:\Apps\Input.ipa" ^
  -f "C:\Tweaks\Tweak.dylib" "C:\Tweaks\Extension.appex" "C:\Tweaks\Package.deb" ^
  -uwdsq -c 9 --overwrite -o "C:\Apps\Output.ipa"
```

For PowerShell, enter that command on one line or replace `^` with a backtick.

| Flag | Action |
| --- | --- |
| `-u` | Remove the supported-device list |
| `-w` | Remove Watch content |
| `-d` | Enable document and file sharing |
| `-s` | Ad-hoc sign with `ldid` |
| `-q` | Thin binaries to arm64 |
| `-e` | Remove app extensions |
| `-g` | Remove encrypted extensions |

Run `cyan --help` for every option. Create a reusable package with:

```text
cgen -o Tweaks.cyan -f Tweak.dylib Resources.bundle -n "My App"
```

CI and release packages include the pinned official Windows build of Procursus `ldid` 2.1.5-procursus7. `--ldid PATH` overrides it.

## Build

Use Developer PowerShell for Visual Studio 2022 with the x64 C++ workload, CMake 3.28+, Ninja, and Git:

```powershell
git clone https://github.com/microsoft/vcpkg ..\vcpkg
git -C ..\vcpkg checkout cd61e1e26a038e82d6550a3ebbe0fbbfe7da78e3
$env:VCPKG_ROOT = (Resolve-Path ..\vcpkg)
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap-vcpkg.ps1 -VcpkgRoot $env:VCPKG_ROOT

powershell -ExecutionPolicy Bypass -File .\scripts\fetch-ldid.ps1 -OutputPath .\out\tools\ldid.exe
$ldid = (Resolve-Path .\out\tools\ldid.exe).Path

cmake --preset windows-release -DCYAN_LDID_EXECUTABLE="$ldid"
cmake --build --preset windows-release
ctest --preset windows-release
cmake --install out\build\windows-release --prefix out\package
```

## Release status

Automated tests cover the CLI, archives, plist data, Mach-O injection, DEB extraction, `.cyan` files, the package pipeline, and real `ldid` signing of generated fixtures.

Before a stable release, the project still needs:

- clean MSVC build, test, smoke-test, and packaging runs on fresh Windows hosts;
- differential output tests against cyan 1.4.4;
- representative authorized IPA, DEB, dylib, framework, and extension tests;
- installation and launch checks on supported iOS devices;
- large-package, low-disk-space, Unicode, long-path, malformed-input, fuzz, memory, and performance tests.

Until these pass, "buildable" does not mean "compatibility verified."

## Benchmarking against cyan

Compatibility and speed need separate results:

1. Freeze a SHA-256-identified corpus covering small, medium, and large packages plus metadata, injection, DEB, extension, `.cyan`, signing, thinning, and Unicode cases.
2. Run cyan 1.4.4 in WSL2 and `cyan-cpp` natively on the same Windows machine, using identical flags, one warm-up, and at least ten fresh runs.
3. Record wall time, CPU time, peak memory, output size, exit code, versions, hardware, and the raw CSV or JSON results.
4. Compare normalized file trees, plist values, Mach-O commands, dependencies, entitlements, and signatures. Ignore ZIP timestamps, entry order, compression bytes, and random icon names.

Because the two programs use different runtime environments, this measures the complete user experience rather than only Python versus C++ execution.

## Credits

Technical acknowledgement goes to [pyzule-rw](https://github.com/asdfzxcvbn/pyzule-rw), [Azule](https://github.com/mpelteshki/Azule), [LIEF](https://github.com/lief-project/LIEF), [insert_dylib](https://github.com/Tyilo/insert_dylib), and [Procursus ldid](https://github.com/ProcursusTeam/ldid).

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency details. Use this project only with apps and binaries you are authorized to modify.
