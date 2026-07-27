# cyan-cpp

[![Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml)

A native C++20 rewrite of [cyan](https://github.com/asdfzxcvbn/pyzule-rw) for Windows x64. It modifies authorized `.ipa`, `.tipa`, and `.app` packages without Python, WSL, macOS, or Xcode at runtime.

> [!WARNING]
> cyan-cpp 0.2.0 is a beta release and is not yet verified as a drop-in replacement for cyan 1.4.4 or ipapatch on a representative IPA corpus. The project leaves beta when it reaches 1.0.0. Keep every input file.

## What it does

- Injects `.dylib`, `.deb`, `.framework`, `.bundle`, and `.appex` content.
- Applies ordered `.cyan` packages and metadata changes.
- Handles thin and FAT Mach-O files, arm64 thinning, entitlements, and `ldid` signing.
- Supports spaces and Unicode characters in Windows paths.
- Includes [ipapatch v2.1.3](https://github.com/asdfzxcvbn/ipapatch/tree/v2.1.3) behavior as a reusable backend and standalone CLI.

## Download

Open [Windows CI](https://github.com/canpng/cyan-cpp/actions/workflows/ci.yml), select a successful run, and download `cyan-cpp-windows-2022-x64`. The ZIP includes `cyan.exe`, `cgen.exe`, `ipapatch.exe`, `ldid.exe`, and the pinned payload.

## Cyan

```text
cyan -i input.ipa -f Tweak.dylib -uwdsq -c 9 --overwrite -o output.ipa
```

Multiple `-f` paths are separated by spaces, not commas. Short flags may be grouped:

| Flag | Action |
| --- | --- |
| `-u` | Remove the supported-device list |
| `-w` | Remove Watch content |
| `-d` | Enable document sharing |
| `-s` | Ad-hoc sign with `ldid` |
| `-q` | Thin Mach-O files to arm64 |
| `-e` | Remove app extensions |
| `-g` | Remove encrypted extensions |

Run `cyan --help` for the complete option list.

## ipapatch

Integrated mode shares Cyan’s open package directory, so the IPA is extracted once and packaged once:

```text
cyan -i input.ipa -f Tweak.dylib --ipapatch --overwrite -o output.ipa
```

Standalone mode uses the same C++ backend:

```text
ipapatch --input input.ipa --output output.ipa --noconfirm
```

Useful options:

```text
--dylib CustomPayload.dylib
--plugins-only
--inplace
```

`--inplace` first creates a complete temporary result and atomically replaces the input only after success. Integrated equivalents are `--ipapatch-dylib` and `--ipapatch-plugins-only`.

The backend patches the main executable and valid immediate `.appex` bundles in `PlugIns` or `Extensions`, skips Watch content, installs the payload in the main app’s `Frameworks` directory, and adds a weak `@rpath/<payload>` load command. Each executable’s signature profile is inspected separately before mutation. XML entitlements, identifier, supported flags, and platform are restored in the final `ldid` pass; DER entitlements are regenerated from the preserved XML and verified to be present.

The bundled Procursus `ldid` is built from pinned commit `c50e84e` and supports `-tTeamID`. The backend restores and verifies each executable’s Team ID independently. DER-only entitlements, unknown signature flags, and different profiles across FAT slices are still rejected before mutation instead of silently losing metadata.

The bundled `zxPluginsInject.dylib` is from ipapatch v2.1.3:

```text
SHA-256  cd903ea15657cbd356398adcb60c8872c41c29b69acc1a5dfb78a49d6e75dea5
```

## Build

Use Developer PowerShell for Visual Studio 2022 with the x64 C++ workload and CMake 3.28+:

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

## Verification status

Automated fixtures cover CLI parsing, safe archives, Unicode paths, thin/FAT injection, multiple extensions, plugins-only mode, Watch skipping, duplicate detection, per-extension signing profiles, atomic failure behavior, single extract/package operation, and real Procursus `ldid` profile preservation.

The 0.2.0 real-IPA validation passed on decrypted YouTube Music 9.29, Instagram 439.0.0, and TikTok 46.1.0 IPAs. All 5, 9, and 10 eligible executables respectively received the weak load command while preserving their individual identifier and Team ID. Each output retained the source file tree plus only the pinned payload, and all three source hashes remained unchanged. Integrated Cyan + ipapatch also passed with one extract and one package operation.

A stable release still requires clean hosted MSVC runs, differential results against real cyan/ipapatch outputs, representative authorized IPA/device tests, malformed-input fuzzing, low-disk and long-path tests, and measured small/medium/large corpus performance. No speed claim is made without those measurements.

The reproducible three-flow benchmark runner is:

```powershell
.\scripts\benchmark-ipapatch.ps1 `
  -CorpusDirectory C:\IPA-Corpus `
  -CyanExecutable .\cyan.exe `
  -ReferenceIpaPatchExecutable C:\Tools\ipapatch.exe `
  -CppIpaPatchExecutable .\ipapatch.exe
```

It records wall time, CPU time, peak memory, output size, and observed extract/package counts to CSV.

## Credits

Technical acknowledgement goes to [pyzule-rw](https://github.com/asdfzxcvbn/pyzule-rw), [ipapatch](https://github.com/asdfzxcvbn/ipapatch), [Azule](https://github.com/mpelteshki/Azule), [LIEF](https://github.com/lief-project/LIEF), [insert_dylib](https://github.com/Tyilo/insert_dylib), and [Procursus ldid](https://github.com/ProcursusTeam/ldid).

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency details. Use this project only with apps and binaries you are authorized to modify.
