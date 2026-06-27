# Layer Ownership Matrix

## Purpose

This document maps current Aleph3 repository areas to their intended long-term
owner:

- `kernel`
- `sdk`
- `pack`
- `tooling`
- `transitional`

Related documents:

- [Aleph3 Unified Plan](aleph3_unified_plan.md)
- [Kernel Representation Decision](kernel_representation_decision.md)

## Ownership Table

| Area | Current State | Target Owner | Notes |
| --- | --- | --- | --- |
| `include/expr`, `src/expr` | symbolic core | `kernel` | Primary semantic representation basis |
| `include/evaluator`, `src/evaluator` | symbolic evaluator core | `kernel` | Must be cleaned toward symbol-centric kernel contracts |
| `include/parser` | symbolic parser surface | `kernel` | Keep if it remains the symbolic parser; avoid duplicate semantic parsers long-term |
| `include/frontend`, `src/frontend` | trusted-subset frontend | `sdk` | SDK/trusted-subset parsing and diagnostics layer |
| `include/ir` | trusted-subset IR | `sdk` | Internal SDK representation only, not kernel-owned semantics |
| `include/semantics`, `src/semantics` | trusted-subset validation | `sdk` | Schema/policy enforcement belongs above the kernel |
| `include/sdk`, `src/sdk` | host-facing API | `sdk` | Stable embedding surface |
| `include/algebra`, `src/algebra` | early domain math | `pack` | First explicit pack extraction target |
| `include/packs` | registry shell | `kernel` | Registration contracts belong to the kernel; pack implementations do not |
| `include/symbols` | early symbol state | `kernel` | Needs expansion into symbol metadata/definition subsystem |
| `include/normalizer` | normalization logic | `kernel` | Canonical structural behavior belongs in the kernel |
| `include/transforms`, `src/transforms` | symbolic transforms | `kernel` | Keep kernel-owned if transform semantics are structural/global; domain-specific transforms should move to packs later |
| `include/tooling`, `src/tooling` | CLI/support tooling | `tooling` | Consumer layer only |
| `tests/evaluator` | symbolic evaluator tests | `kernel` | Rename or regroup later if needed |
| `tests/sdk` | SDK behavior tests | `sdk` | Keep host-facing contract coverage here |
| `tests/algebra` | algebra behavior tests | `pack` | Should follow algebra pack extraction |
| `tests/parser` | symbolic parser tests | `kernel` | Kernel symbolic parser coverage |
| `tests/frontend` | trusted-subset frontend tests | `sdk` | SDK frontend coverage |
| `tests/tooling` | CLI and helper tests | `tooling` | Consumer-level tests |

## Key Transitional Areas

These areas should be treated as explicitly transitional and should not absorb
major new permanent semantics:

- any code path that deepens `ir::Node` into a second symbolic engine

## Ownership Rules

Use these rules when placing new work:

- if it defines symbolic meaning globally, it belongs to `kernel`
- if it constrains or validates host usage of symbolic capability, it belongs
  to `sdk`
- if it adds domain-specific math over stable kernel contracts, it belongs to
  `pack`
- if it only exposes or exercises existing behavior, it belongs to `tooling`

## Forbidden Placement Patterns

Do not:

- add new domain algorithms directly into the kernel evaluator when they can be
  pack-registered
- treat `include/ir` as a second symbolic AST
- add host-facing policy concerns into kernel evaluation logic
- add symbolic-kernel semantics into CLI or example code

## First Follow-On Moves

1. introduce `aleph3_kernel` as an explicit target
2. extract algebra toward the first pack boundary
3. keep SDK evaluation kernel-backed
