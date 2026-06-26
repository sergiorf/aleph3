# Pack Tests

This directory is reserved for tests that belong to explicit pack-owned math
layers rather than the kernel or SDK.

Examples:

- algebra pack tests after algebra extraction is real
- special-functions pack tests after pack registration is real
- future signal-processing and electrical-engineering pack tests

Do not place tests here just because they are mathematically interesting.
Place a test here only when the underlying implementation is owned by a pack
boundary rather than by the kernel.
