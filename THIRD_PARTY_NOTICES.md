# Third-party notices

This source tree does not contain zsign, Tyilo/insert_dylib,
LeanVel/insert_dylib, Azule, or pyzule-rw's opaque tools. The compatibility
frameworks described below and the pinned ipapatch payload are included as
versioned assets.

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
is defined by the application, and their SHA-256 inventory is recorded in
`assets/dependencies/SHA256SUMS`.

## ProcursusTeam / ldid 2.1.5-procursus7+c50e84e

The project includes a Windows x64 build from Procursus commit
`c50e84e18532044b23ec5e971d55ab0cdd4b6685`, which includes the
`-tTeamID` support needed to preserve real IPA CodeDirectory metadata.
CI verifies SHA-256
`609f1f5503a574679e54595c8742cbcce2b650da227c6fd8798dcf426cd773d3`
and ships the upstream `COPYING` file beside `ldid.exe`.
Source: https://github.com/ProcursusTeam/ldid/tree/c50e84e18532044b23ec5e971d55ab0cdd4b6685

## asdfzxcvbn / ipapatch 2.1.3

The native backend reproduces the behavior researched from the v2.1.3 source.
`assets/ipapatch/v2.1.3/zxPluginsInject.dylib` is the exact embedded payload
from that tag. Its SHA-256 is
`cd903ea15657cbd356398adcb60c8872c41c29b69acc1a5dfb78a49d6e75dea5`.
Source: https://github.com/asdfzxcvbn/ipapatch/tree/v2.1.3

## Referenced projects

The following projects informed compatibility research and may also provide
authorised implementation material:

- asdfzxcvbn / pyzule-rw (The Unlicense)
- asdfzxcvbn / ipapatch
- Al4ise and mpelteshki / Azule
- Tyilo / insert_dylib
- LeanVel / insert_dylib

The project owner confirmed on 2026-07-24 that the permissions needed for
use, modification, integration and redistribution were obtained in advance.
