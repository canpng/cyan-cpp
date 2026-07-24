param(
  [Parameter(Mandatory = $true)]
  [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$version = '2.1.5-procursus7'
$releaseTag = "v$version"
$downloadUrl =
  "https://github.com/ProcursusTeam/ldid/releases/download/$releaseTag/ldid_w64_x86_64.exe"
$noticeUrl =
  "https://raw.githubusercontent.com/ProcursusTeam/ldid/$releaseTag/COPYING"
$expectedExecutableSha256 =
  '77a3f012e09619f8cfb5902eba38a00b973da5561ac592c530efb68155f7e6f3'
$expectedNoticeSha256 =
  '57c8ff33c9c0cfc3ef00e650a1cc910d7ee479a8bc509f6c9209a7c2a11399d6'
$downloadedExecutable = [System.IO.Path]::GetTempFileName()
$downloadedNotice = [System.IO.Path]::GetTempFileName()

try {
  Invoke-WebRequest -Uri $downloadUrl -OutFile $downloadedExecutable
  Invoke-WebRequest -Uri $noticeUrl -OutFile $downloadedNotice

  $actualExecutableSha256 =
    (Get-FileHash -LiteralPath $downloadedExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($actualExecutableSha256 -ne $expectedExecutableSha256) {
    throw "ldid SHA-256 mismatch: expected $expectedExecutableSha256, got $actualExecutableSha256"
  }

  $actualNoticeSha256 =
    (Get-FileHash -LiteralPath $downloadedNotice -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($actualNoticeSha256 -ne $expectedNoticeSha256) {
    throw "ldid COPYING SHA-256 mismatch: expected $expectedNoticeSha256, got $actualNoticeSha256"
  }

  $resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
  $outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutput)
  New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
  Copy-Item -LiteralPath $downloadedExecutable -Destination $resolvedOutput -Force
  Copy-Item -LiteralPath $downloadedNotice `
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
      throw "downloaded ldid executable failed its version check: $versionOutput"
    }
  }
  finally {
    Remove-Item -LiteralPath $versionStdout, $versionStderr -Force -ErrorAction SilentlyContinue
  }

  Write-Host "Verified Procursus ldid $version at $resolvedOutput"
}
finally {
  Remove-Item -LiteralPath $downloadedExecutable, $downloadedNotice `
    -Force -ErrorAction SilentlyContinue
}
