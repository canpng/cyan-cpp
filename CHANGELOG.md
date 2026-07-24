# Changelog

## Unreleased

- Added the initial C++20/MSVC project scaffold.
- Added source, dependency, architecture, attribution, parity and test audits.
- Added structured error/result types, secure archive path validation and
  native Mach-O injection interfaces.
- Added the native IPA/TIPA/app mutation pipeline, DEB and `.cyan` ingestion,
  dependency placement, plist metadata updates and WIC icon generation.
- Added native thin/FAT Mach-O inspection and injection, signature layout
  repair, LIEF fallback, arm64 thinning and dependency rewrites.
- Added an explicit external `ldid.exe` signing backend and bundled cyan
  compatibility frameworks with provenance hashes.
- Added synthetic unit and end-to-end tests for archive, CLI, plist, Mach-O,
  app, IPA/TIPA, DEB, icon and `.cyan` workflows.
- Added Windows CI, release, CodeQL and dependency-audit workflows.
