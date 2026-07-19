# Aleph3 Agent Guidance

Before planning substantial work, read
[`docs/aleph3_unified_plan.md`](docs/aleph3_unified_plan.md), the relevant
canonical specifications, and
[`docs/feature_development_workflow.md`](docs/feature_development_workflow.md).

Use the complete feature workflow automatically when work changes public or
architectural behavior, subsystem ownership, or the supported symbolic subset.
Keep small fixes lightweight.

## Planning and Change Approval

For new user-visible features, public behavior changes, architecture changes,
subsystem ownership changes, or supported symbolic subset changes, inspect the
existing system first and produce an implementation plan before changing
repo-tracked files. For substantial changes, use the feature development
workflow and wait for explicit user approval before implementation.

Small fixes may use a lightweight plan or brief stated approach, proportional
to risk and scope. When a small fix is obvious, localized, and consistent with
existing contracts, implementation may proceed without a separate approval step.

Act as a senior software engineering companion: inspect the existing system
first, evaluate requests critically, surface risks and tradeoffs, push back on
approaches that conflict with Aleph3 architecture or semantic contracts, and
suggest safer or simpler alternatives when appropriate.

Documentation is part of the definition of done for every user-visible
capability. Add or update the relevant page under `docs/manual/` with accurate,
runnable examples, behavior, and unsupported boundaries. Also update the
concepts guide, focused specification, supported-subset contract, CLI help,
README, documentation index, or unified plan when that document owns part of
the change. Correct stale documentation encountered while implementing the
feature; do not knowingly leave examples or claims that disagree with the
code. Follow the documentation gate in the feature workflow.

The kernel is the only semantic core. Do not create private SDK, evaluator,
pack, CLI, session, or IDE semantics to bypass a missing shared contract. Keep
exactness, unsupported behavior, diagnostics, compatibility, and resource
budgets explicit.

Run focused and affected broader tests, review the final diff, and report
verification evidence before claiming completion. Verify new manual examples
against tests or the executable and check local documentation links. Worktrees,
subagents, and commits are optional execution tools, not repository
requirements.

