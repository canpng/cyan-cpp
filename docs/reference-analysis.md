# Reference analysis

Audit date: 2026-07-24

This document records the behaviour used as the compatibility reference for
`cyan-cpp`. The project owner confirmed on 2026-07-24 that the required use,
modification, integration and redistribution permissions for analysed
third-party material have already been obtained.

## Audited revisions

| Project | Revision | Role |
| --- | --- | --- |
| [asdfzxcvbn/pyzule-rw](https://github.com/asdfzxcvbn/pyzule-rw) | `740d3716dcd98c20c000f12cdb88f1f0b2a533a4` (main) and `04a40f2aa65ec9cb36127f74d52ae4ef71377c50` (v1.4.4 tag) | Behavioural reference |
| [mpelteshki/Azule](https://github.com/mpelteshki/Azule) | `56e805b1e1ce9ffda953484727e7d0a12cf50994` | Historical/behavioural reference |
| [LIEF](https://github.com/lief-project/LIEF/releases/tag/0.17.6) | `6f3594f27056b85df51d6ad1c4ca944840ad3612` (0.17.6) | Mach-O C++ backend |
| [Tyilo/insert_dylib](https://github.com/Tyilo/insert_dylib) | `eb7278162af8fcc372e7f2946a2dee6a386b17d8` | Behavioural reference only |
| [LeanVel/insert_dylib](https://github.com/LeanVel/insert_dylib) | `75cb0b693115be5dce212655d0d83abb4e127937` | Behavioural reference only |
| [Microsoft/vcpkg](https://github.com/microsoft/vcpkg) | `cd61e1e26a038e82d6550a3ebbe0fbbfe7da78e3` (2026.06.24) | Reproducible dependency registry |

The v1.4.4 tag is followed on `main` by two compatibility fixes while the
package version remains 1.4.4: `/usr/lib` dependency recognition and
`@loader_path` recognition. `cyan-cpp` intentionally includes these fixes.
The history also shows `.tipa` support in v1.2.3, entitlement handling in
v1.2/v1.3, hidden-file exclusion in v1.2.2, multiple `.cyan` files in v1.4.3,
and dependency-name fixes prompted by
[issue #6](https://github.com/asdfzxcvbn/pyzule-rw/issues/6).

## Python-to-C++ responsibility map

| Python component | Observed responsibility | C++ counterpart |
| --- | --- | --- |
| `setup.py` | Package version 1.4.4; `cyan` and `cgen` console entry points; Python >=3.9 | Native `cyan.exe`, `cgen.exe`; `cyan::version` |
| `cyan/__main__.py` | Defines the complete cyan CLI, rejects Windows, hands parsing to `logic.main` | `src/cli/main.cpp`, `CliParser`, `PipelineOptions` |
| `cyan/logic.py` | Ordered top-level pipeline, temporary directory, output selection and packaging | `CyanPipeline`, `TemporaryWorkspace`, `OutputPackager` |
| `cyan/tbhutils.py` | Input checks, overwrite prompt, IPA/APP staging, DEB extraction, archive creation, `.cyan` parsing, tool discovery | `InputValidator`, `ArchiveService`, `IpaArchive`, `DebArchive`, `CyanArchive`, `ToolLocator` |
| `cyan/tbhtypes/plist.py` | Plist load/save, metadata updates, localized names, extension IDs, merge, document flags | `PlistDocument`, `AppMetadataEditor` |
| `cyan/tbhtypes/app_bundle.py` | Bundle discovery, watch/extension removal, icon replacement, recursive executable operations | `AppBundle`, `IconProcessor` |
| `cyan/tbhtypes/executable.py` | Encryption inspection, signature removal, thinning, dependency discovery/rewrites | `MachOInspector`, `MachODependencyEditor`, `FatBinary`, `ISigningBackend` |
| `cyan/tbhtypes/main_executable.py` | Injection orchestration, rpath, DEB expansion, item placement, entitlement restore, insert_dylib/LIEF selection | `InjectionPlanner`, `InsertDylibEngine`, `LiefMachOBackend`, `DependencyResolver`, `EntitlementManager` |
| `cyan/tbhtypes/leaving_cm.py` | Announces temporary directory cleanup | RAII destructor in `TemporaryWorkspace` |
| `cyan/tools/*` | Platform binaries: `otool`, `lipo`, `install_name_tool`, `insert_dylib`, `ldid` | Direct LIEF/native APIs; bundled official Windows ldid with optional path override |
| `cyan/extras/*` | Prebuilt CydiaSubstrate, Orion and Cephei-family frameworks plus zero-requirements blob | Dependency assets and `--dependency-dir` integration |
| `cgen/__main__.py` | Validates options and writes ZIP-based `.cyan` archives | `CyanArchiveWriter`, `src/cgen/main.cpp` |
| `MANIFEST.in` | Ships `cyan/extras` and `cyan/tools` | Explicit release manifest; no opaque inherited binaries |

## Exact cyan 1.4.4 CLI

`cyan` uses `argparse` with a required `-i/--input`, an optional
`-o/--output`, and these options:

| Option | Arity/type | Behaviour/default |
| --- | --- | --- |
| `-i`, `--input` | one path, required | `.app`, `.ipa`, or `.tipa` |
| `-o`, `--output` | one path | Input is overwritten when absent; an unrecognised suffix gets `.ipa` appended |
| `-z`, `--cyan` | one or more paths | Applied in order |
| `-f` | one or more paths | Deduplicated by basename; the last same-basename input wins |
| `-n` | string | App name |
| `-v` | string | Both version keys |
| `-b` | string | Main and derived extension bundle IDs |
| `-m` | digits/dots string | Minimum OS version |
| `-k` | file | Icon source |
| `-l` | plist file | Merge into `Info.plist` |
| `-x` | plist file | Merge/sign main executable entitlements |
| `-u`, `--remove-supported-devices` | flag | Remove `UISupportedDevices` |
| `-w`, `--no-watch` | flag | Remove Watch content |
| `-d`, `--enable-documents` | flag | Set both document browser and file sharing booleans |
| `-s`, `--fakesign` | flag | Sign all discovered executables |
| `-q`, `--thin` | flag | Thin all discovered binaries to arm64 |
| `-e`, `--remove-extensions` | flag | Remove `Extensions` and `PlugIns` |
| `-g`, `--remove-encrypted` | flag | Remove encrypted first-level `.appex` bundles |
| `-c`, `--compress` | integer 0..9 | ZIP level, default 6 |
| `--ignore-encrypted` | flag | Continue when the main executable has `cryptid=1` |
| `--overwrite` | flag | Do not prompt for an existing output |
| `--version` | flag | Prints `cyan v1.4.4` |

Messages use `[*]` for progress/success, `[?]` for notices, `[!]` for errors,
`[<]` for questions and `[>]` for termination. Existing output is accepted by
`y`, `yes`, or an empty answer. `n` and any other non-affirmative answer quit.

## Pipeline order

The observable order in `cyan/logic.py` is:

1. Normalize input/output and append `.ipa` to an unknown output suffix.
2. Validate input, output collision, injection paths, minimum version, icon,
   merge plist, `.cyan` files and entitlements.
3. Create a temporary root and extract/copy the app into
   `temporary/Payload/<name>.app`.
4. Load `Info.plist`, locate `CFBundleExecutable`, and reject an encrypted main
   executable unless `--ignore-encrypted` is set.
5. Apply each `.cyan` archive in argument order. Later configuration keys
   overwrite earlier CLI/config values; injection dictionaries accumulate and
   collide by basename.
6. Remove all extensions, or only encrypted extensions. This deliberately
   happens before injection so newly supplied `.appex` items survive.
7. Inject/copy `-f` inputs.
8. Apply, in order: name, version, bundle ID, minimum OS, icon, plist merge,
   entitlement merge.
9. Apply, in order: remove supported devices, remove Watch, enable documents,
   fake-sign all, thin all.
10. Write IPA/TIPA or move the `.app` to the destination.
11. Delete the temporary workspace.

The C++ pipeline preserves this order but changes output publication to a
temporary sibling plus atomic replace. A failed operation must not destroy the
original input.

## Injection and placement semantics

Before processing inputs, cyan:

- extracts the main executable's entitlements;
- removes its code signature;
- creates `PlugIns` if an `.appex` exists;
- creates `Frameworks` and attempts to add
  `@executable_path/Frameworks` if a `.deb`, `.dylib`, or `.framework` exists;
- expands every `.deb`, discovering top-level-effective `.dylib`, `.appex`,
  `.bundle`, and `.framework` items while ignoring symlinks and nested
  bundle/framework matches.

Items are then handled in basename-key order:

| Input | Destination | Main executable command |
| --- | --- | --- |
| `.appex` | `PlugIns/<basename>` | None |
| `.dylib` | `Frameworks/<basename>` | Weak `@rpath/<basename>` |
| `.framework` | `Frameworks/<basename>` | Weak `@rpath/<framework>/<stem>` |
| `.bundle` or unknown directory | App root | None |
| Unknown file | App root | None |

Existing targets are removed and replaced. Symlink inputs are skipped.
Copied dylibs have their signatures removed and both common and user
dependencies rewritten before placement. Extracted original entitlements are
restored after all load commands have been written.

## Dependency rules

Candidate dependency paths begin with `/Library/`, `/usr/lib/`, or `@`
(`@rpath`, `@loader_path`, and `@executable_path` are therefore included).
Case-insensitive substring matching maps:

| Match key | Required bundle | Canonical dependency |
| --- | --- | --- |
| `substrate.` | `CydiaSubstrate.framework` | `@rpath/CydiaSubstrate.framework/CydiaSubstrate` |
| `orion.` | `Orion.framework` | `@rpath/Orion.framework/Orion` |
| `cephei.` | `Cephei.framework` | `@rpath/Cephei.framework/Cephei` |
| `cepheiui.` | `CepheiUI.framework` | `@rpath/CepheiUI.framework/CepheiUI` |
| `cepheiprefs.` | `CepheiPrefs.framework` | `@rpath/CepheiPrefs.framework/CepheiPrefs` |

Orion also causes Substrate to be required. User framework dependencies become
`@rpath/<name>.framework/<name>`; other named inputs become
`@rpath/<basename>`.

## Metadata and bundle behaviour

- Name sets both `CFBundleName` and `CFBundleDisplayName`, then tries every
  root-level `*.lproj/InfoPlist.strings`.
- Version sets `CFBundleVersion` and `CFBundleShortVersionString`.
- Bundle ID replaces the main ID, then replaces the old main-ID prefix in
  first-level `*/*.appex/Info.plist` identifiers.
- Minimum OS sets `MinimumOSVersion`.
- Plist merge is shallow, key by key; later values replace complete earlier
  values.
- Documents sets `UISupportsDocumentBrowser` and `UIFileSharingEnabled`.
- Watch removal targets `Watch`, `WatchKit`, and
  `com.apple.WatchPlaceholder`.
- Extension removal targets both `Extensions` and `PlugIns`.
- Executable discovery includes recursive `.dylib`, `.appex`, and `.framework`
  paths. A bundle executable comes from its `Info.plist`.

## Icon behaviour

The source is converted to PNG when needed. Cyan generates a random
`cyan_<7 hex>a` base (the trailing letter avoids a numeric final character),
writes `<base>60x60@2x.png` at 120x120 and
`<base>76x76@2x~ipad.png` at 152x152, and merges:

- `CFBundleIcons/CFBundlePrimaryIcon` with the 60x60 stem;
- `CFBundleIcons~ipad/CFBundlePrimaryIcon` with both stems;
- `CFBundleIconName` set to the random base.

On Windows this maps to Windows Imaging Component rather than Pillow.

## `.cyan` and cgen

A `.cyan` file is a deflated ZIP. Required `config.json` stores only truthy
CLI values. The file-bearing keys `f`, `k`, `l`, and `x` are stored as boolean
`true`; their content is:

| Config key | Archive entry |
| --- | --- |
| `f` | `inject/*` |
| `k` | `icon.idk` |
| `l` | `merge.plist` |
| `x` | `new.entitlements` |

`cgen` accepts `-o/--output`, all mutation options `-f -n -v -b -m -k -l -x`,
and flags `-u/-w/-d/-s/-q/-e/-g`. It appends `.cyan`, prompts before
overwrite, writes compression level 1, and recursively places directory
contents below `inject/<directory-basename>/...`.

## Azule comparison

Azule is a shell-based ancestor with a broader package/download workflow:
repository lookup, MobileAPT/Canister download, optional App Store acquisition
and decryption, tweak filter plist evaluation, hooking-library selection, and
many advanced flags. Its core order is still recognisable: unpack app and
DEBs, identify executable/rpath, extract entitlements/remove signature,
discover and filter tweak payloads, copy frameworks/bundles/appex, rewrite
install names and dependencies, inject load commands, restore entitlements,
apply metadata/removals, and repack.

Cyan deliberately simplifies this model: local inputs, basename collision
semantics, fixed common dependency mappings, weak main-executable injection,
and a shareable `.cyan` layer. No Azule shell source is used in this project.

## Reference limitations carried forward only in compatibility mode

The Python implementation has unsafe extraction (`extractall`), non-atomic
overwrite, weak return-code checking, shallow plist merge, lossy executable
discovery, basename collisions, and inconsistent hidden-file behaviour
depending on whether the external `zip` binary exists. `cyan-cpp` reproduces
the visible successful-case semantics but does not reproduce vulnerabilities
or silent corruption. Differences are surfaced as structured errors.
