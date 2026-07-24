# Test strategy

## Principles

Tests use generated fixtures and do not require a commercial IPA or opaque app
binary. Bundled compatibility frameworks are release inputs with a separate
hash inventory; unit and integration fixtures do not execute them. A
regression first receives a failing test and a minimal fixture, then a fix.

## Unit suites

| Suite | Required cases |
| --- | --- |
| Checked arithmetic/endian | 16/32/64 swaps, checked add/multiply, alignment overflow |
| Mach-O magic/header | All thin/FAT magics, both endian modes, truncated headers |
| FAT | FAT32/FAT64 entries, overlap, misalignment, range overflow, multiple slices |
| Load commands | Zero/small/oversized `cmdsize`, table overflow, unknown command |
| Dylib injection | 8-byte alignment, strong/weak, duplicate, zero padding, insufficient/non-zero space |
| Signature repair | Last/not-last signature, non-terminal blob, missing/non-terminal `__LINKEDIT`, 32/64 sizes |
| Symtab repair | Valid trailing padding, invalid range, overflow |
| Verification | Modified output reparses and contains the exact requested command in every slice |
| Inspection | architecture, dependencies, rpaths, `cryptid`, signature state |
| Plist | XML/binary, every value type, invalid/truncated data, round trip |
| Metadata | name/localisation, versions, extension IDs, shallow merge, document flags |
| Archive path | slash/backslash traversal, drive/UNC/device/ADS names, trailing dot/space, case collisions |
| Archive quotas | entry/file/total/ratio limits and cleanup |
| `.cyan` | required config, all payload names, ordered overlays, malformed/missing entries |
| Dependency resolver | Substrate aliases, Orion implication, Cephei ordering, user inputs |
| CLI | every short/long option, arity, compression range, Windows path forms, prompts |

## Synthetic Mach-O fixtures

`tests/tools/macho_fixture_builder` constructs byte arrays from constants
defined by the test, not copied binaries. Fixtures cover:

- thin 32-bit and thin arm64 64-bit;
- FAT arm64+x86_64 and FAT64;
- both endian encodings;
- exact duplicate weak/strong commands;
- signed terminal `__LINKEDIT` layout with string-table padding;
- insufficient command padding;
- encrypted `cryptid=1`;
- malformed arithmetic and boundary cases.

Generated binaries live in each test's temporary directory. A small checked-in
JSON description may be used, but generated binary output is not source.

## Integration tests

1. Construct a minimal `Payload/Synthetic.app` with generated plist and Mach-O.
2. Package it as IPA and TIPA with Unicode and long entry names.
3. Run each mutation individually and in a combined pipeline.
4. Reopen the output with a fresh service instance.
5. Compare the semantic manifest and executable ZIP mode bits.
6. Inject a synthetic DEB for each supported `data.tar.*` filter.
7. Exercise existing-output atomic replacement and injected failures.
8. Verify no files remain outside the workspace after malicious archives.

## Differential compatibility

Where Python cyan can legally and safely run, `tests/compatibility` invokes it
against the same synthetic app. `cyan-inspect` emits normalised JSON containing:

- bundle-relative file tree and file kinds;
- semantic plist values;
- executable modes;
- Mach-O slices, load commands, rpaths, dependencies and encryption;
- entitlements/signature state;
- extension and Watch inventories;
- `.cyan` entry tree and `config.json`.

Timestamps, ZIP entry order, random icon base names, compression byte streams,
padding and other non-semantic details are normalised. A Python dependency is
allowed only in this optional reference test job, never in released binaries.

## Fuzzing and security

Parser entry points accept an in-memory byte span so they can be fuzzed without
filesystem setup. Seed corpora are generated fixtures. Targets include Mach-O
container parsing, plist parse, archive-name validation and `.cyan`
configuration. CI runs deterministic malformed-input corpora; scheduled jobs
can run libFuzzer/ASan on a supported LLVM runner.

## Windows verification matrix

Required CI build:

- `windows-2022`, Release;
- MSVC `/W4 /permissive- /Zc:__cplusplus /EHsc`, project warnings as errors;
- CTest, CLI `--help`/`--version`, package smoke test;
- paths containing Turkish/CJK characters, spaces, UNC syntax and a
  greater-than-260-character opt-in path;
- release dependency/DLL inventory and SHA-256.

CodeQL runs independently. Release tags repeat Release tests before packaging.
Debug and newer-runner validation may be added temporarily when investigating
toolchain-specific defects; they are not required to produce the user artifact.

## Exit criteria

A feature advances:

- to **Implemented** when production code and negative-path handling exist;
- to **Tested** when deterministic unit/integration tests pass on Windows;
- to **Compatibility verified** only after a normalised differential test
  passes against cyan 1.4.4.

The build itself is not evidence of behavioural compatibility.
