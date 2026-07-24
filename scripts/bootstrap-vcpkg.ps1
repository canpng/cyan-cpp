param(
  [Parameter(Mandatory = $true)]
  [string]$VcpkgRoot,

  [ValidateRange(1, 10)]
  [int]$Attempts = 4
)

$ErrorActionPreference = 'Stop'

$resolvedRoot = [System.IO.Path]::GetFullPath($VcpkgRoot)
$bootstrapPath = Join-Path $resolvedRoot 'bootstrap-vcpkg.bat'
$metadataPath = Join-Path $resolvedRoot 'scripts\vcpkg-tool-metadata.txt'
$executablePath = Join-Path $resolvedRoot 'vcpkg.exe'

if (-not (Test-Path -LiteralPath $bootstrapPath -PathType Leaf)) {
  throw "vcpkg bootstrap script not found: $bootstrapPath"
}
if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) {
  throw "vcpkg tool metadata not found: $metadataPath"
}

$metadata = ConvertFrom-StringData (Get-Content -LiteralPath $metadataPath -Raw)
$expectedRelease = $metadata.VCPKG_TOOL_RELEASE_TAG
if ([string]::IsNullOrWhiteSpace($expectedRelease)) {
  throw "VCPKG_TOOL_RELEASE_TAG is missing from $metadataPath"
}

function Test-CompatibleVcpkg {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedRelease
  )

  if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    return $false
  }

  try {
    $versionOutput = (& $Executable version --disable-metrics 2>&1 | Out-String)
    $versionExitCode = $LASTEXITCODE
  }
  catch {
    return $false
  }

  return $versionExitCode -eq 0 -and
    $versionOutput.IndexOf(
      $ExpectedRelease,
      [System.StringComparison]::Ordinal
    ) -ge 0
}

if (Test-CompatibleVcpkg -Executable $executablePath -ExpectedRelease $expectedRelease) {
  Write-Host "Using cached vcpkg tool release $expectedRelease."
  return
}

Remove-Item -LiteralPath $executablePath -Force -ErrorAction SilentlyContinue
$lastFailure = 'unknown bootstrap failure'

for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
  Write-Host "Bootstrapping vcpkg tool release $expectedRelease (attempt $attempt/$Attempts)..."

  try {
    & $bootstrapPath -disableMetrics
    $bootstrapExitCode = $LASTEXITCODE
  }
  catch {
    $bootstrapExitCode = if ($LASTEXITCODE) { $LASTEXITCODE } else { 1 }
    $lastFailure = $_.Exception.Message
  }

  if (
    $bootstrapExitCode -eq 0 -and
    (Test-CompatibleVcpkg -Executable $executablePath -ExpectedRelease $expectedRelease)
  ) {
    Write-Host "vcpkg tool release $expectedRelease is ready."
    return
  }

  if ($bootstrapExitCode -ne 0) {
    $lastFailure = "bootstrap-vcpkg.bat exited with code $bootstrapExitCode"
  }
  else {
    $lastFailure = "downloaded vcpkg.exe does not report release $expectedRelease"
  }

  Remove-Item -LiteralPath $executablePath -Force -ErrorAction SilentlyContinue

  if ($attempt -lt $Attempts) {
    $delaySeconds = [Math]::Min(5 * $attempt, 30)
    Write-Warning "$lastFailure; retrying in $delaySeconds seconds."
    Start-Sleep -Seconds $delaySeconds
  }
}

throw "Unable to bootstrap vcpkg tool release $expectedRelease after $Attempts attempts: $lastFailure"
