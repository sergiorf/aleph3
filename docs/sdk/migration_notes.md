# Migration Notes

The repository is currently in a split state:

- the legacy prototype remains buildable and testable
- the SDK path now has explicit SDK and IR boundaries
- the SDK implementation is still ahead of some legacy-adjacent cleanup work

## Recommended Developer Workflow

1. Add new implementation under SDK targets first.
2. Reuse legacy code only after isolating the dependency and documenting why it is safe.
3. Add SDK-path tests for any promoted behavior before wiring it into the SDK surface.
4. Keep product-facing documentation centered on the embedded-engine wedge, not the old broad CAS behavior.

## Promotion Checklist

- Does the feature belong in Trusted Subset V1?
- Can it be expressed without exposing legacy `Expr`?
- Does it have explicit diagnostic behavior?
- Can it be bounded by schema or policy?
- Does it have SDK-path tests?
