# Aleph3 Documentation

This page is the entry point and ownership map for the documentation. Each
topic has one canonical home; focused specifications may link to it instead of
repeating its background.

## Start Here

For the complete user journey, start with the
[Aleph3 Manual](manual/README.md). It covers expressions, built-ins, rewriting,
the SDK, packs, sessions, the CLI, and the current notebook and paused web
foundations. The same sources can be
[built as a PDF book](manual/README.md#build-the-pdf-book).

1. [Project README](../README.md) - build, run, and product orientation.
2. [Architecture](architecture.md) - system shape and ownership boundaries,
   including the current kernel, pack, session, notebook-core, SDK, and paused
   web service architecture.
3. [SDK Guide](sdk/README.md) - embedding surface and SDK-specific references.
4. [Notebook MVP Design](notebook_mvp_design.md) - product scope, shipped
   headless notebook-core slices, planned GUI behavior, evaluation,
   persistence, display, and acceptance contract.
5. [Unified Plan](aleph3_unified_plan.md) - active implementation roadmap,
   including the Windows-first local notebook MVP.
6. [Web MVP Launch Plan](web_mvp_launch_plan.md) - paused web launch scope,
   anonymous-user strategy, API shape, deployment phases, and acceptance gates
   for a possible future web notebook. It is not the active roadmap.
7. [Web MVP Operations](web_mvp_operations.md) - local service, port,
   Docker Compose, Traefik, and smoke-test procedures for the existing paused
   web slice.
8. [IP and Repository Strategy](ip_and_repo_strategy.md) - practical
   public/private transition guidance.

## Normative References

These documents define current behavior rather than retelling the whole
architecture.

| Area | Canonical documents |
| --- | --- |
| SDK language | [Trusted subset](trusted_subset_v1.md), [stable interfaces](sdk/stable_interfaces.md) |
| Kernel and engine structure | [Architecture](architecture.md), [Kernel design](kernel_design_spec.md), [execution bridge](kernel_execution_bridge_spec.md) |
| Symbols | [Symbol model](kernel_symbol_model_spec.md), [definition precedence](kernel_symbol_definition_precedence.md), [attributes](kernel_attribute_spec.md), [variable analysis](kernel_variable_analysis_spec.md), [list and structural operations](kernel_list_structural_spec.md) |
| Rewriting | [Rewrite specification](kernel_rewrite_spec.md) |
| Exact mathematics | [Exact algebra](kernel_exact_algebra_spec.md), [supported algebra subset](algebra_supported_subset.md), [algebra equivalence](algebra_equivalence_spec.md), [dense matrices](algebra_dense_matrix_spec.md), [focused differentiation](calculus_differentiation_spec.md) |
| Assumptions | [Assumptions specification](kernel_assumptions_spec.md) |
| Registration | [Registration lifecycle](kernel_registration_lifecycle_spec.md) |
| Quality | [Feature development workflow](feature_development_workflow.md), [contract test matrix](contract_test_matrix.md), [header documentation guideline](header_documentation_guideline.md) |
| Build and tests | [Build and targets](sdk/build_and_targets.md), [testing strategy](sdk/testing_strategy.md), [Windows Codex build environment](agents/windows-codex-build.md) |

## Document Types

- **Guide** documents teach readers how the system fits together.
- **Specification** documents state testable current contracts and limits.
- **Plan** documents track unfinished work and sequencing. A paused plan may
  remain in the tree when it documents an implemented transitional surface or
  an intentionally deferred product path.
- **Archive** documents under [docs/archive](archive/README.md) preserve completed
  implementation plans that are no longer canonical current-behavior
  references.

A specification should link to architecture terminology rather than reproduce
it. A plan may point at a specification, but does not override it. When code,
a specification, and a plan disagree, treat that as a defect to resolve - not a
reason to add a fourth explanation.

## Maintenance Rule

Before adding a document, decide whether the material belongs in an existing
guide, specification, or plan. Prefer a section and a stable anchor over a new
top-level file. Keep historical discussion in Git history; keep the current
tree focused on facts readers still need.

Remove a document only when it is obsolete, unreferenced, and no longer useful
as a canonical specification, current guide, active or paused plan, operational
runbook, or archived implementation record. When a superseded document still
explains a completed decision or migration, move it under `docs/archive/`
instead of deleting it.
