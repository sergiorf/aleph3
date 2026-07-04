# Feature Development Workflow

This workflow turns an Aleph3 feature idea into verified implementation work
without creating another roadmap. The [Unified Plan](aleph3_unified_plan.md)
remains authoritative for direction, priority, milestones, and sequencing.
Focused specifications remain authoritative for subsystem contracts.

The default path for substantial work is:

> roadmap alignment -> repository research -> design approval ->
> implementation plan -> test-led execution -> review and verification

The workflow applies equally to human contributors and coding agents. It borrows
useful discipline from plan-driven development systems without requiring a
particular tool, worktree strategy, commit cadence, or use of subagents.

## Classify The Change

Classify work before deciding how much process it needs.

### Trivial Fix

A local correction with no new behavior, public contract, architectural
decision, or supported-subset change. Examples include a typo, an obviously
incorrect diagnostic, or a narrowly understood regression.

Inspect the affected code and tests, make the smallest justified change, run
focused verification, and review the diff. A separate design or durable plan is
not required.

### Bounded Enhancement

A contained behavioral change whose owner and contract are already clear. It
may need a short written plan in the issue, pull request, or active task, but it
does not need a new design document when the canonical specification already
settles the important decisions.

Confirm roadmap compatibility, inspect the contract and implementation, state
success and non-goals, then execute with tests and verification.

### Substantial Feature

Use the complete workflow when a change does any of the following:

- introduces or materially changes public or architectural behavior
- creates a concept, extension point, representation, or ownership boundary
- changes the supported symbolic subset or unsupported fallback behavior
- crosses kernel, SDK, pack, session, CLI, or future IDE boundaries
- changes exactness, diagnostics, registration, evaluation, or compatibility
- requires unresolved product or implementation decisions

When uncertain, begin with research. Reclassify the change after the existing
contracts and implementation are understood.

## Align With The Unified Plan

Before designing a substantial feature, record:

- the milestone and workstream it advances
- its relationship to the current priority order
- whether it belongs to the active tranche or allowed parallel work
- the owning subsystem: kernel, SDK, pack, session, CLI, or tooling
- whether it conflicts with or depends on deferred work
- its effect, if any, on the three-track program balance

The balanced kernel/SDK, math-pack, and interactive tracks are a program-level
scheduling rule. Do not make one feature touch all three tracks artificially.
Instead, state which track owns the feature and which related tranche work
keeps the overall program balanced.

A feature that conflicts with the accepted core decisions or deferred-work
boundaries needs an explicit roadmap decision before implementation. Do not
hide that decision in a subsystem plan.

## Research The Repository

Build the design from repository evidence rather than architectural guesses.
Inspect and cite in the working notes or feature plan:

- the relevant architecture, specification, and supported-subset documents
- current public interfaces and subsystem ownership boundaries
- implementation entry points, data flow, and extension mechanisms
- existing unit, contract, integration, and process-level tests
- diagnostics, fallback behavior, resource budgets, and failure recovery
- compatibility constraints and explicitly unsupported cases

Resolve discoverable questions from code and documentation first. Escalate only
product choices, tradeoffs, or genuine conflicts that repository evidence
cannot settle.

The research must enforce these invariants:

- the kernel remains the only semantic core
- SDK policy constrains kernel semantics instead of duplicating them
- packs register behavior through shared kernel contracts
- CLI and session consumers reuse kernel evaluation and diagnostics
- exact operations do not silently fall back to approximate foundations
- unsupported behavior is explicit and deterministic
- user-visible failures produce coherent diagnostics

## Approve The Design

For substantial features, agree on a design before producing the task plan.
The design states:

- the goal and observable user or contributor outcome
- explicit non-goals and unsupported cases
- user-visible behavior with representative examples
- subsystem ownership and affected public contracts
- data flow, interfaces, and lifecycle behavior
- validation, failure behavior, diagnostics, and recovery
- exactness and compatibility expectations
- viable alternatives and why the chosen approach is preferred

Record decisions in the canonical subsystem specification when they define a
durable contract. A transient design note may support discussion, but it must
not become a competing source of truth.

## Write A Decision-Complete Plan

A substantial feature plan must be executable without asking the implementer
to make product or architecture decisions. Record:

1. Goal and observable success criteria.
2. Unified Plan milestone, workstream, priority, and tranche impact.
3. Kernel, SDK, pack, session, or CLI ownership and affected public contracts.
4. Current implementation evidence and relevant canonical specifications.
5. Chosen design, rejected alternatives, and explicit non-goals.
6. Ordered implementation tasks with tests and verification for each task.
7. Compatibility, unsupported behavior, diagnostics, documentation, and the
   completion checklist.

Organize the plan into small vertical tasks. Each task names:

- the behavior or contract it delivers
- likely implementation and test locations
- prerequisite tasks or decisions
- the test to add or change
- documentation that must change
- focused verification commands and expected evidence

Every user-visible vertical task includes its documentation work in that task.
Documentation is not an optional cleanup step after implementation.

Prefer semantic milestones over mechanical file-edit lists. Include file names
when they prevent ambiguity, not as a substitute for describing behavior.

## Execute Through Quality Gates

Use these gates in order, adapting the test-first step only when it is genuinely
impractical for documentation-only work or the available harness:

1. **Baseline:** run the relevant existing tests and record unrelated failures.
2. **Red:** add or identify a focused test that demonstrates the missing or
   incorrect behavior, and confirm that it fails for the expected reason.
3. **Green:** implement the smallest coherent vertical slice that satisfies the
   approved contract.
4. **Focused verification:** run the closest unit, contract, or integration
   tests after each task.
5. **Broader verification:** run the affected target suite and cross-surface
   tests where behavior spans SDK, pack, CLI, or session boundaries.
6. **Review:** inspect the complete diff for duplicate semantics, accidental API
   growth, unsupported fallbacks, missing diagnostics, and stale documentation.
7. **Completion evidence:** report commands run, results, remaining limitations,
   and any deliberate deviations from the plan.

Never claim completion from code inspection alone when executable verification
is available. A passing test does not override an incorrect architectural
boundary or undocumented public behavior.

## Keep Canonical Documents Canonical

Update the Unified Plan only when roadmap priority, milestone state,
sequencing, or the immediate action queue changes. Put durable subsystem
behavior in its focused specification and explanatory material in a guide.
Update supported-subset documentation whenever user-visible support or
unsupported behavior changes.

Do not copy the workflow into feature plans, specifications, or pull requests;
link here and record only the feature-specific decisions and evidence.

## Documentation Gate

Every new or changed user-visible capability must leave the documentation
consistent with shipped behavior. Review this ownership map and update every
applicable document:

- **Manual:** update the relevant chapter under `docs/manual/` with purpose,
  syntax, runnable examples, evaluation behavior, diagnostics, exact/inexact
  behavior, and unsupported cases. Extend an existing chapter before creating
  another one.
- **Concepts appendix:** update
  `docs/manual/concepts-and-terminology.md` when the feature introduces
  vocabulary or a mental model needed to understand its syntax.
- **Specifications and supported subsets:** record durable contracts,
  invariants, failure behavior, and precise boundaries in their canonical
  focused documents.
- **CLI help and discovery:** update help text, completion metadata, examples,
  and command descriptions when the feature is interactively visible.
- **README and documentation index:** update these only for onboarding,
  navigation, build, or major product-surface changes; do not duplicate the
  manual there.
- **Unified Plan:** update it only when delivery changes roadmap state,
  priorities, milestones, or the immediate action queue.

Documentation must accompany the implementation that makes it true. Do not
present planned behavior as current behavior; label future notebook, graph,
pack, or SDK functionality explicitly as direction or planned work.

When mathematical context helps explain a capability, include the relevant
concept, definition, or identity in the manual alongside a runnable Aleph3
example. State assumptions, exactness requirements, conventions, and
unsupported boundaries explicitly. Mathematical background should illuminate
the shipped contract rather than imply capabilities the engine does not have.

Validate documentation proportionally:

- execute representative manual examples or cover them with tests
- ensure displayed canonical output matches the current renderer
- check local Markdown links and headings
- search the affected area for stale claims contradicted by the feature
- make limitations and failure behavior as visible as successful examples

Correct stale documentation found in the affected area in the same change. If
the correction is materially broader than the feature, report that scope
expansion explicitly.

## Completion Checklist

A substantial feature is complete when:

- its observable success criteria pass
- kernel, SDK, pack, session, and CLI ownership remains consistent
- exactness, diagnostics, budgets, and unsupported behavior are explicit
- focused and affected broader tests pass, or failures are accounted for
- public contracts and supported-subset documentation match the code
- relevant manual pages explain the capability with verified examples and
  explicit limitations
- affected concepts, help text, README navigation, and links are current
- the diff contains no unnecessary duplicate semantics or unrelated changes
- roadmap status is updated only if the delivered work changed it
- completion evidence and remaining limitations are reported

## Example Walks

### Queued Traversal And Replacement Controls

This is a substantial feature because it expands symbolic behavior and may
affect matching, evaluation, diagnostics, and the supported subset. Map it to
the active kernel/SDK queue and rewrite workstream; verify that sequence-pattern
machinery remains out of scope. Research the rewrite specification, evaluator
entry points, expression representation, and existing replacement tests.
Approve position-selection semantics, traversal order, held-expression
behavior, invalid-control diagnostics, and termination budgets before planning.
Then divide implementation into testable vertical slices such as position
selection, bounded traversal, replacement integration, diagnostics, and public
documentation. This yields a plan grounded in existing contracts rather than
asking the implementer to invent traversal semantics.

### Small Regression Fix

An existing supported expression returning the wrong diagnostic, with no
contract ambiguity, is a trivial fix. Locate the owning implementation and
existing diagnostic contract, add a focused regression test, make the local
correction, run the affected suite, and review the diff. It needs no roadmap
mapping document, alternatives analysis, durable feature plan, or artificial
three-track work.
