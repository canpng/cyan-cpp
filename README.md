# cyan-cpp

`cyan-cpp` is a C++20 rewrite of
[asdfzxcvbn/pyzule-rw](https://github.com/asdfzxcvbn/pyzule-rw) for native
Windows x64. The target command-line programs are `cyan.exe` and `cgen.exe`.
Python, WSL, macOS, Xcode, zsign and Unix command-line tools are not runtime
dependencies.

> **Development status:** the native IPA/TIPA/app pipeline, CLI programs,
> archive and plist layers, WIC icon processing, dependency placement, native
> Mach-O injection, LIEF fallback and external `ldid.exe` adapter are enabled.
> Synthetic end-to-end tests cover app, IPA/TIPA, DEB, `.cyan`, metadata, icon
> and dylib workflows. A differential corpus and hosted MSVC release evidence
> are still required before declaring drop-in compatibility.

## Supported Windows versions

- Windows 10 x64
- Windows 11 x64
- MSVC from Visual Studio Build Tools 2022 or newer

The first supported release platform is Windows. Core parsers avoid Windows
types, while path publication, UTF conversion and WIC image operations are
Windows adapters.

## Implemented features

- cyan 1.4.4-compatible option parsing and messages
- native UTF-16 command line and `std::filesystem::path` handling
- structured errors and RAII temporary workspaces
- ZIP/AR/TAR/filter support through libarchive, without running external tools
- archive traversal, device-name, ADS, duplicate-case, symlink/reparse and
  decompression-limit checks
- Python-compatible `cgen` options and `.cyan` archive layout
- Substrate, Orion and Cephei-family dependency mappings
- Mach-O 32/64 and FAT32/FAT64 parsing in both byte orders
- architecture, dependency, rpath, encryption and signature inspection
- native strong/weak dylib injection with duplicate and padding checks
- terminal `LC_CODE_SIGNATURE`, `__LINKEDIT` and `LC_SYMTAB` repair
- FAT slice rebuilding with offset, size and alignment updates
- independent post-write parsing and atomic output replacement
- LIEF 0.17.6 C++ fallback using a byte-vector parser for Unicode-safe paths
- end-to-end IPA, TIPA and app-directory staging and publication
- DEB discovery through AR and nested `data.tar.*` archives
- XML/binary plist metadata, extension, watch and document-option handling
- Windows Imaging Component icon decode, resize and PNG encoding
- ordered `.cyan` reading, overlay application and file payloads
- arm64 thinning, bundled compatibility frameworks and optional `ldid.exe`
  signing

The precise implementation state is in
[docs/feature-parity-matrix.md](docs/feature-parity-matrix.md).

## Compatibility with cyan

The behavioural baseline is cyan 1.4.4 plus the two post-tag fixes that still
identify as 1.4.4 (`/usr/lib` and `@loader_path` dependencies). Processing
order, basename collision rules, metadata keys, `.cyan` overlay order and CLI
prefixes are documented in
[docs/reference-analysis.md](docs/reference-analysis.md).

Security bugs are not compatibility targets. The C++ implementation does not
reproduce unsafe `extractall`, silent subprocess failures, non-atomic
in-place writes or unchecked Mach-O arithmetic.

## Known differences

- Archive links and Windows reparse points are currently rejected rather than
  materialised.
- The archive writer consistently excludes hidden IPA content when requested;
  Python cyan changed behaviour depending on the presence of external `zip`.
- `ldid.exe` is not redistributed. `-x`/`--fakesign` and entitlement
  extraction require an explicit `--ldid` path; a missing signer is a hard
  error. Injection without signing removes stale signatures but cannot restore
  their entitlements.
- The GitHub MSVC workflows are defined, but their hosted results are not
  claimed by this source checkout.

## Download from GitHub Actions

Open the repository's **Actions** tab, choose a successful **Windows CI** run,
and download `cyan-cpp-windows-2022-x64`. The regular CI workflow deliberately
uses one Windows 2022 Release build. CodeQL is weekly/manual, and tagged
releases run only for `v*` tags.

## Download from Releases

Tags matching `v*` create:

- `cyan-cpp-windows-x64.zip`
- `cyan-cpp-windows-x64.sha256`
- `cyan-cpp-windows-x64.spdx.json`

Do not treat a pre-1.0 archive as compatibility-complete; consult the parity
matrix included by the corresponding source revision.

## Local build

Install Visual Studio Build Tools with the x64 C++ workload, CMake 3.28+,
Ninja, Git and PowerShell. Clone the pinned vcpkg revision and set
`VCPKG_ROOT`:

```powershell
git clone https://github.com/microsoft/vcpkg
git -C vcpkg checkout cd61e1e26a038e82d6550a3ebbe0fbbfe7da78e3
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = (Resolve-Path .\vcpkg)

cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
cmake --install out\build\windows-release --prefix out\package
```

Dependencies are pinned by `vcpkg-configuration.json` and `vcpkg.json`. Do not
update LIEF without running synthetic injection and differential compatibility
tests.

## GitHub Desktop workflow

1. Clone the repository in GitHub Desktop.
2. Make source or documentation changes.
3. Review the exact diff and commit it.
4. Push the current branch.
5. Inspect Windows CI in GitHub Actions.
6. Download the artifact only after all matrix jobs pass.

No submodule initialisation is required.

## cyan CLI

```text
cyan -i input.ipa [-o output.ipa] [options]
```

Examples for the compatibility target:

```powershell
cyan -i App.ipa -o App-patched.ipa -f Tweak.dylib --overwrite
cyan -i App.tipa -z Base.cyan DeviceOverrides.cyan -c 9
cyan -i App.app -n "New Name" -b dev.example.new -w -e
cyan -i App.ipa -f Tweak.deb --dependency-dir C:\cyan-dependencies
```

Run `cyan --help` for the complete option list. These examples execute through
the native pipeline in the current build.

## cgen CLI

```powershell
cgen -o SocialTweaks.cyan -f Tweak.dylib Resources.bundle -n "Social"
cgen -o Documents.cyan -d -u -l ExtraInfo.plist --overwrite
```

`cgen` is functional in the current build and creates a ZIP-compatible `.cyan`
file at compression level 1.

## `.cyan` format

Every archive contains `config.json`. File-bearing options use these names:

```text
config.json
inject/*
icon.idk
merge.plist
new.entitlements
```

Multiple archives are applied in argument order. Later scalar configuration
values replace earlier values; injection entries accumulate and resolve
basename collisions using cyan-compatible ordering.

## Dependency architecture

- LIEF 0.17.6: public C++ Mach-O modification/fallback
- libarchive 3.8.7: ZIP, AR, TAR, gzip, xz, zstd and bzip2
- libplist 2.7.0: XML and binary plist processing
- nlohmann/json 3.12.0: `.cyan` configuration
- Catch2 3.15.1: tests only
- Windows Imaging Component: icon decode, resize and PNG encode
- bundled Cephei, CepheiUI, CepheiPrefs, CydiaSubstrate and Orion compatibility
  frameworks: resolved when the corresponding cyan dependency flags are used

See [docs/dependency-map.md](docs/dependency-map.md) and
[docs/architecture.md](docs/architecture.md).

## Signing backend status

Signing is deliberately separated from injection. The implemented backend is
an explicit `ldid.exe` adapter supporting entitlement extraction and ad-hoc
signing; native and LIEF code remove obsolete signatures before mutation. No
code path reports a successful fake-sign operation without a configured
backend and verification.

## Security considerations

Only modify apps and binaries you are authorised to use. Treat every IPA, DEB,
`.cyan`, plist, image and Mach-O file as hostile. Work on copies until a
release is compatibility-verified. Report archive escapes, integer overflows
and output-replacement issues privately as described in `SECURITY.md`.

## Legal and authorised-use notice

This project is intended for legitimate development, interoperability,
research, testing and authorised app customisation. Users are responsible for
complying with platform terms, local law and rights in the apps they process.

## Credits and acknowledgements

Technical thanks to:

- asdfzxcvbn / [pyzule-rw](https://github.com/asdfzxcvbn/pyzule-rw)
- Al4ise and mpelteshki / [Azule](https://github.com/mpelteshki/Azule)
- lief-project / [LIEF](https://github.com/lief-project/LIEF)
- Tyilo / [insert_dylib](https://github.com/Tyilo/insert_dylib)
- LeanVel / [insert_dylib](https://github.com/LeanVel/insert_dylib)
- ProcursusTeam / [ldid](https://github.com/ProcursusTeam/ldid)

The project owner confirmed that the permissions required for analysed and
integrated third-party materials were obtained before development.

## Third-party notices

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Release archives include
the exact notices collected from the locked dependency installation.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) and
[docs/test-strategy.md](docs/test-strategy.md). New parser behaviour requires
a synthetic negative test; compatibility claims require differential evidence.
