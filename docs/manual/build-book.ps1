<#
Builds the Aleph3 manual from its canonical Markdown chapters.
Requires Pandoc and XeLaTeX; generated output stays under build/docs.
#>

[CmdletBinding()]
param(
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"
$manualDirectory = $PSScriptRoot
$repositoryRoot = (Resolve-Path (Join-Path $manualDirectory "../..")).Path

if (-not $OutputPath) {
    $OutputPath = Join-Path $repositoryRoot "build/docs/aleph3-manual.pdf"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path (Get-Location) $OutputPath
}

if (-not (Get-Command pandoc -ErrorAction SilentlyContinue)) {
    throw "Pandoc was not found. Install it from https://pandoc.org/installing.html"
}
if (-not (Get-Command xelatex -ErrorAction SilentlyContinue)) {
    throw "XeLaTeX was not found. Install MiKTeX or TeX Live and ensure xelatex is on PATH."
}

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

Push-Location $manualDirectory
try {
    & pandoc --defaults book.yaml --output $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "Pandoc failed with exit code $LASTEXITCODE."
    }
} finally {
    Pop-Location
}

Write-Host "Built $OutputPath"
