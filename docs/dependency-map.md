# Dependency map

Registry baseline:
[`microsoft/vcpkg@cd61e1e2`](https://github.com/microsoft/vcpkg/commit/cd61e1e26a038e82d6550a3ebbe0fbbfe7da78e3).
Versions below are the exact versions at that baseline.

## Runtime/build replacements

| Python cyan or Unix dependency | Native replacement | Version / linkage | Purpose |
| --- | --- | --- | --- |
| Python `zipfile`, `zip`, `unzip` | [libarchive](https://github.com/libarchive/libarchive) | 3.8.7, vcpkg | ZIP IPA/TIPA/`.cyan` read/write |
| Unix `ar`, `tar` | libarchive | 3.8.7 | DEB AR and `data.tar.{gz,xz,zst,bz2}` |
| `otool -L`, `otool -l` | Native parser + [LIEF](https://github.com/lief-project/LIEF/releases/tag/0.17.6) | 0.17.6 / commit `6f3594f...` | Mach-O inspection and independent verification |
| `install_name_tool -change` | LIEF C++ `DylibCommand::name` | 0.17.6 | Dependency path replacement |
| `install_name_tool -add_rpath` | LIEF C++ `RPathCommand::create` + `Binary::add` | 0.17.6 | `@executable_path/Frameworks` |
| `lipo -thin arm64` | Native FAT parser or LIEF `FatBinary::take` | 0.17.6 fallback | arm64 extraction |
| external `insert_dylib` | Native `InsertDylibEngine` + LIEF fallback | Project code + LIEF 0.17.6 | Strong/weak load commands |
| Python `plistlib` | [libplist](https://github.com/libimobiledevice/libplist) C API | 2.7.0, shared DLL only | XML and binary plist |
| Python `json` | [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | `.cyan` configuration and inspection manifests |
| Pillow | Windows Imaging Component | Windows SDK | Decode, resize and PNG encode icons |
| `ldid` | Bundled official [Procursus ldid](https://github.com/ProcursusTeam/ldid) | 2.1.5-procursus7 Windows x64, SHA-256 pinned | Entitlement preservation and ad-hoc signing |
| Python `tempfile` | `TemporaryWorkspace` | Project code | RAII cleanup |
| Python `os.path`/`shutil` | `std::filesystem` + Win32 atomic replace | C++20/Windows | Unicode paths and safe publication |
| Python `argparse` | Project CLI parser | Project code | Exact short/long option compatibility without an added dependency |
| Python test tooling | [Catch2](https://github.com/catchorg/Catch2) | 3.15.1 | Unit and fixture tests |

## Selected vcpkg features

- `lief`: `default-features: false`; Mach-O support remains enabled by the
  vcpkg port, while logging/DEX/OAT/VDEX/ART/JSON are unnecessary.
- `libarchive`: default features provide bzip2, lzma and zstd. XAR and crypto
  are not needed and should be disabled to reduce the binary/dependency set.
- `libplist`: tools disabled. The initial integration uses the shared library
  build so the executable package remains modular.
- `nlohmann-json`: no optional features.
- `catch2`: test-only; no runtime distribution.

## CMake target map

| Package | CMake package | Imported target |
| --- | --- | --- |
| LIEF | `find_package(LIEF CONFIG REQUIRED)` | `LIEF::LIEF` |
| libarchive | `find_package(LibArchive REQUIRED)` | `LibArchive::LibArchive` |
| libplist | `find_package(unofficial-libplist CONFIG REQUIRED)` | `unofficial::libplist::libplist` |
| nlohmann/json | `find_package(nlohmann_json CONFIG REQUIRED)` | `nlohmann_json::nlohmann_json` |
| Catch2 | `find_package(Catch2 3 CONFIG REQUIRED)` | `Catch2::Catch2WithMain` |

## Dependency boundaries

`cyan_core` contains platform-independent data models and CLI infrastructure.
`cyan_macho` has no LIEF dependency. `cyan_lief` is the sole adapter that
includes LIEF headers. `cyan_archive` owns libarchive, `cyan_plist` owns
libplist, `cyan_image` owns WIC/COM, and `cyan_signing` owns process creation
for the bundled or explicitly configured ldid signer.
`cyan_pipeline` composes those adapters.

No dependency may be fetched from a mutable branch. vcpkg's baseline pins port
recipes and upstream checksums. LIEF is additionally pinned to release 0.17.6
and its exact upstream commit in the audit.
