# Feature parity matrix

Status values are **Not started**, **In progress**, **Implemented**, **Tested**
and **Compatibility verified**. “Tested” means deterministic tests pass in the
local Windows LLVM/MinGW validation build. Hosted MSVC and differential cyan
1.4.4 results are tracked separately and are not inferred from local success.

| Area | Feature | Status | Notes/evidence |
| --- | --- | --- | --- |
| Build | C++20 Windows x64 project | Tested | Strict local build with warnings as errors |
| Build | `cyan.exe` and `cgen.exe` native entry points | Tested | Help/version smoke tests |
| Build | No Python/WSL/macOS runtime | Implemented | Native libraries and Win32 adapters only |
| Build | No zsign or Unix command dependency | Implemented | Optional signer is explicit `ldid.exe` |
| Input/output | `.ipa` and `.tipa` | Tested | Synthetic TIPA-to-IPA pipeline round trip |
| Input/output | `.app` directory | Tested | Synthetic bundle transformation |
| Input/output | Atomic output and overwrite policy | Tested | Sibling temporary publication |
| Input/output | ZIP compression levels 0-9 | Implemented | Validated by CLI and libarchive writer |
| Paths | Unicode/UTF-16 Windows paths | Tested | Native wide entry points and filesystem paths |
| Workspace | RAII temporary cleanup | Tested | Pipeline fixtures exercise success cleanup |
| Security | Traversal, rooted, ADS and device-name rejection | Tested | Archive validator negative suite |
| Security | Link/reparse and case-fold collision rejection | Tested | Extraction policy and validator suite |
| Security | Entry, file, total and expansion quotas | Implemented | Enforced during streaming extraction |
| Injection | Multiple items and basename last-wins | Implemented | Ordered planner mirrors cyan semantics |
| Injection | `.deb` AR and nested `data.tar.*` | Tested | Synthetic DEB end-to-end test |
| Injection | `.dylib` | Tested | Placement plus load-command verification |
| Injection | `.framework`, `.bundle`, `.appex` | Implemented | Type-specific placement paths |
| Injection | Unknown item to app root | Implemented | Compatibility placement rule |
| Mach-O | Thin 32/64 and FAT32/FAT64, both byte orders | Tested | Generated fixture suite |
| Mach-O | Strong/weak load commands and duplicates | Tested | Native injector tests |
| Mach-O | Command-space and zero-padding validation | Tested | Positive and negative fixtures |
| Mach-O | Signature, `__LINKEDIT` and `LC_SYMTAB` repair | Tested | Synthetic signed-layout fixtures |
| Mach-O | FAT slice relocation and alignment | Tested | FAT fixtures and independent reparse |
| Mach-O | Independent post-write verification | Tested | Inspector validates every written slice |
| Mach-O | LIEF 0.17.6 fallback | Implemented | Public byte-vector parser/write API |
| Inspection | Architectures, dependencies, rpaths | Tested | Native inspector unit/integration tests |
| Inspection | Encryption and signature state | Tested | Generated load-command fixtures |
| Inspection | arm64 thinning | Implemented | LIEF `FatBinary::take` adapter |
| Dependencies | Substrate aliases and Orion implication | Tested | Resolver unit suite |
| Dependencies | Cephei family and user rewrites | Tested | Resolver unit suite |
| Dependencies | Bundled compatibility frameworks | Implemented | Packaged assets plus SHA-256 inventory |
| Plist | XML and binary read/write | Tested | libplist round-trip tests |
| Plist | Name, version, bundle ID and minimum iOS | Tested | End-to-end bundle test |
| Plist | Localisations and extension bundle IDs | Implemented | Bundle traversal and strings update |
| Plist | Device removal and document flags | Tested | Metadata tests |
| Plist | Shallow external merge | Tested | Plist unit suite |
| Bundle | Watch and all-extension removal | Tested | Synthetic bundle test |
| Bundle | Encrypted-extension removal | Implemented | Nested executable inspection |
| Bundle | Nested executable discovery | Implemented | Main and extension bundle traversal |
| Icon | WIC decode, resize and PNG encode | Tested | Synthetic BMP input |
| Icon | 120x120, 152x152 and plist metadata | Tested | End-to-end icon assertions |
| Cyan | Ordered multiple archive application | Implemented | Sequential reader application |
| Cyan | `config.json` | Tested | `cgen` round trip |
| Cyan | `inject/*`, `icon.idk`, `merge.plist`, entitlements | Implemented | Reader/writer payload mapping |
| Cgen | Python-compatible arguments and ZIP layout | Tested | Parser and archive round-trip suites |
| Signing | `ISigningBackend` separation | Implemented | Pipeline depends on interface |
| Signing | Entitlement extraction/merge/restore | Implemented | External adapter; real signer not locally available |
| Signing | Native/LIEF signature removal | Tested | Mach-O fixture suite |
| Signing | Ad-hoc signing | Implemented | Explicit `ldid.exe` adapter; requires user executable |
| Testing | Synthetic thin/FAT/app/IPA/DEB fixtures | Tested | Single deterministic Catch2 executable |
| Testing | Differential cyan manifest | Not started | Required for “Compatibility verified” |
| CI | Windows 2022/2025 Debug/Release definitions | Implemented | Workflow authored; hosted run not claimed |
| CI | CTest and CLI smoke tests | Implemented | Workflow steps and local equivalents |
| Release | ZIP, SHA-256 and SPDX SBOM | Implemented | Tag workflow; hosted run not claimed |
| Release | CodeQL | Implemented | Dedicated workflow |
| Attribution | Credits, notices and bundled-asset provenance | Implemented | README, notices and asset hash inventory |

No row is marked **Compatibility verified** until the differential harness has
run against cyan 1.4.4 with a representative corpus.
