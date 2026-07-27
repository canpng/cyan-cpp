param(
  [Parameter(Mandatory = $true)]
  [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$version = '2.1.5-procursus7+c50e84e'
$assetDirectory =
  Join-Path (Split-Path -Parent $PSScriptRoot) 'assets\ldid\c50e84e'
$sourceExecutable = Join-Path $assetDirectory 'ldid.exe'
$sourceNotice = Join-Path $assetDirectory 'COPYING'
$expectedExecutableSha256 =
  '609f1f5503a574679e54595c8742cbcce2b650da227c6fd8798dcf426cd773d3'
$expectedNoticeSha256 =
  '282751b8c98ee9e445346eb57a992c9ecbe25ed8dd554df046777313e19b10f9'

if (-not (Test-Path -LiteralPath $sourceExecutable -PathType Leaf) -or
    -not (Test-Path -LiteralPath $sourceNotice -PathType Leaf)) {
  throw "bundled ldid assets are missing from $assetDirectory"
}

$actualExecutableSha256 =
  (Get-FileHash -LiteralPath $sourceExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualExecutableSha256 -ne $expectedExecutableSha256) {
  throw "ldid SHA-256 mismatch: expected $expectedExecutableSha256, got $actualExecutableSha256"
}

$actualNoticeSha256 =
  (Get-FileHash -LiteralPath $sourceNotice -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualNoticeSha256 -ne $expectedNoticeSha256) {
  throw "ldid COPYING SHA-256 mismatch: expected $expectedNoticeSha256, got $actualNoticeSha256"
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutput)
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
Copy-Item -LiteralPath $sourceExecutable -Destination $resolvedOutput -Force
Copy-Item -LiteralPath $sourceNotice `
  -Destination (Join-Path $outputDirectory 'ldid.COPYING') -Force

$versionStdout = [System.IO.Path]::GetTempFileName()
$versionStderr = [System.IO.Path]::GetTempFileName()
try {
  $process = Start-Process -FilePath $resolvedOutput -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput $versionStdout -RedirectStandardError $versionStderr
  $versionOutput =
    (Get-Content -Raw -LiteralPath $versionStdout) +
    (Get-Content -Raw -LiteralPath $versionStderr)
  if ($versionOutput -notmatch [regex]::Escape($version)) {
    throw "bundled ldid executable failed its version check: $versionOutput"
  }
}
finally {
  Remove-Item -LiteralPath $versionStdout, $versionStderr -Force -ErrorAction SilentlyContinue
}

& $resolvedOutput -t
if ($LASTEXITCODE -ne 0) {
  throw 'bundled ldid does not support -tTeamID'
}

Write-Host "Verified Procursus ldid $version at $resolvedOutput"
