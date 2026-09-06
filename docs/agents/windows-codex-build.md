# Windows Codex Build Environment

When building Aleph3 from a Codex-managed PowerShell on Windows, sanitize the
process environment before invoking MSBuild through CMake. The managed shell may
expose user-profile paths under `%USERPROFILE%` and duplicate `Path`/`PATH`
variables in ways that cause Visual Studio's C++ `FileTracker` to fail before
compilation.

Prefer the existing `build` directory with a sanitized process-only environment
instead of creating a second build tree just to avoid MSBuild:

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
