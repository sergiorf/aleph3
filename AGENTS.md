# Aleph3 Agent Guidance

Before planning substantial work, read
[`docs/aleph3_unified_plan.md`](docs/aleph3_unified_plan.md), the relevant
canonical specifications, and
[`docs/feature_development_workflow.md`](docs/feature_development_workflow.md).

Until the Web MVP is completed, interpret user references to "the plan" as
[`docs/web_mvp_launch_plan.md`](docs/web_mvp_launch_plan.md) by default. Keep
the unified plan as the longer-term roadmap, but do not switch planning focus
back to it unless the user explicitly asks for the longer-term plan or the Web
MVP plan is complete.

Use the complete feature workflow automatically when work changes public or
architectural behavior, subsystem ownership, or the supported symbolic subset.
Keep small fixes lightweight.

## Planning and Change Approval

For new user-visible features, public behavior changes, architecture changes,
subsystem ownership changes, or supported symbolic subset changes, inspect the
existing system first and produce an implementation plan before changing
repo-tracked files. For substantial changes, use the feature development
workflow and wait for explicit user approval before implementation.

If the user changes scope, architecture, persistence model, deployment target,
or another public behavior direction during an active task, stop before further
repo-tracked edits. Re-plan from the new direction, state what existing local
changes are now pending or potentially obsolete, and wait for explicit user
approval before continuing implementation.

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

When completing a fix or implementation, include suggested commit text in the
final response: a concise commit subject and, when useful, a short body that
summarizes the behavioral change and verification evidence.

## Windows Build Environment

When building Aleph3 from a Codex-managed PowerShell on Windows, sanitize the
process environment before invoking MSBuild through CMake. The managed shell may
expose user-profile paths under `%USERPROFILE%` and duplicate `Path`/`PATH`
variables in ways that cause Visual Studio's C++ `FileTracker` to fail before
compilation. Prefer the existing `build` directory with a sanitized process-only
environment instead of creating a second build tree just to avoid MSBuild:

```powershell
$repo = (Get-Location).Path
$base = Join-Path $repo 'build\codex-msbuild-env'
New-Item -ItemType Directory -Force -Path `
  $base, "$base\AppData\Roaming", "$base\AppData\Local", "$base\Temp", "$base\OneDrive" | Out-Null

$env:APPDATA = "$base\AppData\Roaming"
$env:LOCALAPPDATA = "$base\AppData\Local"
$env:TEMP = "$base\Temp"
$env:TMP = "$base\Temp"
$env:OneDrive = "$base\OneDrive"
$env:OneDriveConsumer = "$base\OneDrive"
$env:CODEX_MANAGED_PACKAGE_ROOT = $repo

[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
$env:Path = (@(
  'C:\windows\system32',
  'C:\windows',
  'C:\windows\System32\Wbem',
  'C:\windows\System32\WindowsPowerShell\v1.0',
  'C:\windows\System32\OpenSSH',
  'C:\Program Files\Git\cmd',
  'C:\Program Files\CMake\bin',
  'C:\Program Files\dotnet'
) | Where-Object { Test-Path -LiteralPath $_ }) -join ';'

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  throw 'cmake is not available after sanitizing Path; add the local CMake path before building.'
}

cmake --build (Join-Path $repo 'build') --config Release --target <target>
```

If extra tools are needed, add only machine-level tool directories that exist on
the current machine to `$env:Path`; avoid reintroducing `%USERPROFILE%` entries
for the build process.

