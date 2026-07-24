# Third-party notices

This source tree does not contain zsign, Tyilo/insert_dylib,
LeanVel/insert_dylib, Azule, or pyzule-rw's opaque tools. The compatibility
frameworks described below are included in `assets/dependencies`.

The build resolves dependencies from the pinned vcpkg registry in
`vcpkg-configuration.json`. Release packaging must copy the exact licence
texts from `vcpkg_installed/<triplet>/share/<port>/copyright`.

## LIEF 0.17.6

Copyright LIEF contributors. Licensed under Apache License 2.0.
Source: https://github.com/lief-project/LIEF/tree/0.17.6

## libarchive 3.8.7

Copyright libarchive contributors. Licensed under the project's
BSD-2-Clause-style licence and applicable file-level notices.
Source: https://github.com/libarchive/libarchive/tree/v3.8.7

## nlohmann/json 3.12.0

Copyright Niels Lohmann and contributors. Licensed under the MIT License.
Source: https://github.com/nlohmann/json/tree/v3.12.0

## libplist 2.7.0

Copyright libimobiledevice contributors. Used through the public C API for
XML and binary plist processing.
Source: https://github.com/libimobiledevice/libplist/tree/2.7.0

## Catch2 3.15.1

Copyright Catch2 contributors. Licensed under the Boost Software License 1.0.
Catch2 is used only to build tests.
Source: https://github.com/catchorg/Catch2/tree/v3.15.1

## Bundled compatibility frameworks

Cephei, CepheiUI, CepheiPrefs, CydiaSubstrate and Orion framework payloads are
carried forward from the cyan 1.4.4 reference tree at commit
`04a40f7141fcc8ec6ad1e3ec8a389d870072e6e3`. Their release-package placement
and SHA-256 inventory are recorded in `assets/dependencies/README.md` and
`assets/dependencies/SHA256SUMS`.

## ProcursusTeam / ldid 2.1.5-procursus7

The Windows x64 release package includes the official
`ldid_w64_x86_64.exe` for entitlement extraction and ad-hoc Mach-O signing.
CI verifies SHA-256
`77a3f012e09619f8cfb5902eba38a00b973da5561ac592c530efb68155f7e6f3`
and ships the upstream `COPYING` file beside `ldid.exe`.
Source: https://github.com/ProcursusTeam/ldid/releases/tag/v2.1.5-procursus7

## Referenced projects

The following projects informed compatibility research and may also provide
authorised implementation material:

- asdfzxcvbn / pyzule-rw (The Unlicense)
- Al4ise and mpelteshki / Azule
- Tyilo / insert_dylib
- LeanVel / insert_dylib

The project owner confirmed on 2026-07-24 that the permissions needed for
use, modification, integration and redistribution were obtained in advance.
