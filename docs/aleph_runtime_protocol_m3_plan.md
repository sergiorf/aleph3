# Aleph Runtime Protocol M3 Plan

## Status

This is the active detailed implementation plan for M3 from
[Public App / Private Kernel Split](public_private_cli_split_plan.md): a
process-launching runtime client library over the implemented private
`aleph-runtime` protocol server.

M1, the protocol model, and M2, the private runtime executable, are complete
and archived in [docs/archive](archive/README.md).

## Goal

Add a public-client-facing process layer that can locate and launch
`aleph-runtime`, exchange framed protocol requests and responses, handle
timeouts and process exit, and report missing or incompatible runtimes with
stable diagnostics.

The slice succeeds when client tests can drive a fake runtime and the real
runtime through the same framed lifecycle without linking private kernel,
parser, evaluator, session, pack, or CLI implementation headers.

## Non-Goals

- migrating `aleph3_cli` through the runtime client;
- public notebook integration;
- physical repository movement;
- cancellation, streaming output, or concurrent requests;
- host-function registration over the protocol;
- adding new symbolic semantics or protocol methods beyond client lifecycle
  needs.

## Implementation Direction

The client library belongs above `aleph_client` and must remain independent of
private semantic targets. Runtime lookup follows the split-plan order:
explicit path, `ALEPH_RUNTIME_PATH`, same directory as the client executable,
then `PATH`.

Tests should cover missing runtime diagnostics, incompatible protocol version
diagnostics, request/response round trips, process exit before response,
timeout handling, malformed runtime output, and shutdown cleanup. Fake-runtime
tests are the primary public-client proof; real-runtime smoke coverage may be
private.

## Completion Criteria

M3 is complete when the process-launching client can be built without private
semantic headers, fake-runtime lifecycle tests pass, real-runtime smoke tests
pass where private targets are enabled, and documentation explains runtime
lookup and failure behavior without claiming CLI migration or notebook
integration.
