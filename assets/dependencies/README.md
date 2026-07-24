# Bundled compatibility frameworks

These five framework bundles are the runtime compatibility assets used by
cyan 1.4.4 for common tweak dependencies:

- `CydiaSubstrate.framework`
- `Orion.framework`
- `Cephei.framework`
- `CepheiUI.framework`
- `CepheiPrefs.framework`

They were imported from `asdfzxcvbn/pyzule-rw` tag `v1.4.4`, commit
`04a40f2aa65ec9cb36127f74d52ae4ef71377c50`. The project owner confirmed the
required integration and redistribution permissions on 2026-07-24.

`SHA256SUMS` records the executable and top-level plist identities. The
release package installs this directory next to `cyan.exe`; an explicit
`--dependency-dir` takes precedence.
