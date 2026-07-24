# Contributing

Use C++20, RAII, `std::filesystem::path`, structured errors and target-local
compiler settings. Do not introduce global mutable state, owning raw pointers,
shell command construction or mutable dependency references. Third-party
material must be identified in `THIRD_PARTY_NOTICES.md`.

For each behaviour change:

1. add a synthetic failing test;
2. implement the smallest complete fix;
3. run the configured build and CTest presets;
4. update `docs/feature-parity-matrix.md` without overstating verification.

Format-only changes should be kept separate from behavioural changes.
