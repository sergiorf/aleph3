# Aleph3 Architecture

## Purpose

This is the stable high-level architecture document for Aleph3.

It is intentionally short and durable.
It explains the long-lived structure of the system without depending on
transitional implementation details.

For roadmap and sequencing, see
[Aleph3 Unified Plan](aleph3_unified_plan.md).

For detailed implementation structure, see:

- [Symbolic Core Architecture](symbolic_core_architecture.md)
- [System Architecture](system_architecture.md)
- [Kernel Design Spec](kernel_design_spec.md)
- [SDK Stable Interfaces](sdk/stable_interfaces.md)

## System Shape

Aleph3 has four long-lived layers:

1. kernel
2. packs
3. SDK
4. tools and products

```text
tools / products
        |
       SDK
        |
      packs
        |
      kernel
```

## Plain-Language Terms

These are the core terms used across the docs.

- kernel
  The math engine itself. If Aleph3 needs to know what `x + x` means, how
  `If[...]` behaves, or whether `1/2 + 1/3` stays exact, that logic belongs in
  the kernel.
- pack
  An add-on math library on top of the kernel. A future calculus pack could
  add rules such as `D[x^2, x] -> 2*x` without changing the kernel core.
- SDK
  The embedding API for applications. If a host app wants to validate a
  formula, supply variables, and run it safely, it goes through the SDK.
- rewrite
  A rule-based expression change. Example: `f[x] -> g[x]` rewrites `f[f[x]]`
  into `g[g[x]]`.
- host function
  A function implemented by the embedding application instead of Aleph3.
  Example: an app may register `Clamp` or `PriceForSku` and let formulas call
  it during evaluation.

## Kernel

The kernel is the semantic center of Aleph3.

It owns:

- symbolic expression representation
- evaluation semantics
- symbol metadata and definition storage
- rewrite and transformation infrastructure
- exact arithmetic foundations
- diagnostics
- registration contracts used by built-ins, packs, and host integration

The kernel must not be treated as a private implementation detail under the
SDK. All other layers depend on its semantics.

## Packs

Packs are domain libraries built on top of the kernel.

They should add:

- algebra algorithms
- special functions
- calculus rules
- solver logic
- other domain-specific symbolic behavior

They should not bypass kernel contracts by hardcoding domain growth directly
into evaluator internals.

## SDK

The SDK is the host-facing embedding layer.

It owns:

- `Engine`
- `Schema`
- `Policy`
- public value and diagnostic types
- trusted-subset parsing and validation
- operational controls such as budgets and allowlists

The SDK constrains and packages kernel behavior for host applications.
It does not own a second semantic runtime.

## Tools And Products

Tools and products sit above the SDK and kernel:

- CLI
- examples
- future notebook-style applications
- hosted or commercial surfaces

They must reuse unified kernel semantics rather than introduce separate
execution models.

## Execution Flow

There are two important execution paths.

### SDK Path

```text
source
-> frontend parser
-> trusted-subset IR
-> semantics validation
-> lowering into kernel Expr
-> kernel evaluation
-> SDK Value / RuntimeError
```

### Symbolic Path

```text
symbolic source
-> Expr
-> kernel evaluation / normalization / rewrite
-> Expr
```

Both paths are converging on the same kernel semantics.

## Rewrite, In Plain Terms

A rewrite is a symbolic edit step driven by a rule.

Example:

```text
f[x] -> g[x]
```

Applied to:

```text
f[f[x]]
```

Result:

```text
g[g[x]]
```

Practical examples:

- rewrite can simplify shape: `0 + x -> x`
- rewrite can normalize shape: `x + x -> 2*x` within a limited supported class
- rewrite is different from plain numeric evaluation: `2 + 3 = 5` is numeric
  reduction, while `f[x] -> g[x]` is structural transformation

In Aleph3 today, rewrite is a kernel subsystem used for controlled symbolic
transformations. It already supports exact structural rules, simple named
patterns like `a_`, and a narrow arithmetic simplification layer.

## Stable Architectural Rules

- There must be one kernel, not two peer semantic runtimes.
- `Expr` is the long-term kernel representation.
- `ir::Node` is an SDK-side representation for trusted-subset parsing and
  validation, not a second kernel AST.
- Domain math should grow through packs over kernel contracts.
- The SDK should remain a small, stable adoption layer.
- Interactive applications should be consumers of kernel and SDK behavior, not
  new semantic centers.

## What Should Feel Stable To Readers

These statements should remain true even as code moves:

- Aleph3 is kernel-first.
- The SDK is public and host-facing.
- Packs are the extension path for domain growth.
- Rewrite is part of symbolic transformation infrastructure.
- Products sit above the kernel rather than redefining it.
