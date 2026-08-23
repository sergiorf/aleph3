# IP and Repository Strategy

## Scope

This is practical project guidance, not legal advice. Obtain counsel before
changing distribution terms, accepting outside contributions, or making a
major distribution-licensing change.

The current repository is licensed under the Apache License 2.0. Earlier
copies already distributed under the MIT license remain available under the
MIT terms under which recipients received them; publishing a later license
does not revoke rights already granted.

Future versions can use Apache License 2.0 where the author owns the relevant
copyright or otherwise has permission to license the work that way.
Contributions and third-party dependencies can complicate future relicensing,
so contribution and dependency provenance should stay clear.

## Recommended Transition

Do not split the repository merely to create the appearance of protection.
First stabilize the kernel/session/product interfaces in the current tree and
inventory copyright ownership, contributor agreements, dependency licenses,
release artifacts, and CI secrets.

Before any later change in repository or distribution strategy:

1. tag and archive the last public MIT release and the first Apache 2.0
   release;
2. record a source and dependency provenance audit;
3. create access-controlled repositories for future kernel, notebook, and
   separately distributed work if the project ever needs that structure;
4. keep public documentation truthful about which binaries and source are available;
5. define a release process that never publishes private submodules, symbols,
   debug artifacts, package feeds, or CI logs accidentally.

A possible later layout is:

```text
aleph3-core-private
aleph3-notebook-private
aleph3-packs-private
aleph3-site-public
aleph3-examples-public
aleph3-demo-public
```

This is a deployment option, not today's repository structure. Public examples
should depend on a versioned binary/product interface rather than copying
advanced simplification, solving, code-generation, or notebook internals.

## Boundaries and Risks

- Architecture secrecy is not a substitute for stable APIs, signing, license
  compliance, backups, and access control.
- A public demo should expose a deliberately limited contract, not compile the
  private kernel into a redistributable artifact by accident.
- The project needs an explicit policy for accepting contributions before any
  later relicensing or separately distributed implementation is attempted.
- Qt and every other GUI/runtime dependency require a distribution-license
  review after the toolkit and packaging model are selected.
- Marketing, examples, screenshots, and documentation may remain public while
  implementation stays private, but public claims must match shipped behavior.

The strategic principle is simple: preserve implementation options while
keeping the kernel as the sole semantic core. A repository split must never
cause a demo, SDK, notebook, or pack to grow duplicate semantics.
