# Security policy

## Supported versions

Until the first stable release, only the latest commit on the default branch
receives security fixes.

## Reporting

Do not attach proprietary IPAs, signing identities, passwords, provisioning
profiles, or private entitlements to a public issue. Report a vulnerability
privately through GitHub's security-advisory interface when enabled, or contact
the repository maintainers privately.

Include a minimal synthetic archive or Mach-O fixture when possible.

## Threat model

IPA, DEB, `.cyan`, plist, image and Mach-O inputs are untrusted. Relevant
classes include traversal, symlink/junction escape, decompression bombs,
integer overflow, malformed load commands, unsafe output replacement and
shell argument injection. See `docs/architecture.md`.

