param(
  [Parameter(Mandatory = $true)]
  [string]$PackageRoot,

  [ValidateRange(1, 30)]
  [int]$GuiStartupSeconds = 5
)

$ErrorActionPreference = 'Stop'

$resolvedRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$requiredFiles = @(
  'cyan-gui.exe'
  'cyan.exe'
  'cgen.exe'
  'ipapatch.exe'
  'ldid.exe'
  'zxPluginsInject.dylib'
  'Qt6Core.dll'
  'double-conversion.dll'
  'md4c.dll'
  'pcre2-16.dll'
  'msvcp140.dll'
  'vcruntime140.dll'
  'vcruntime140_1.dll'
  'Qt6\plugins\platforms\qwindows.dll'
  'qt.conf'
)

foreach ($relativePath in $requiredFiles) {
  $path = Join-Path $resolvedRoot $relativePath
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    throw "$relativePath is missing from the Windows package"
  }
}

$payloadPath = Join-Path $resolvedRoot 'zxPluginsInject.dylib'
$payloadHash =
  (Get-FileHash -LiteralPath $payloadPath -Algorithm SHA256).Hash.ToLowerInvariant()
$expectedPayloadHash =
  'cd903ea15657cbd356398adcb60c8872c41c29b69acc1a5dfb78a49d6e75dea5'
if ($payloadHash -ne $expectedPayloadHash) {
  throw "pinned ipapatch payload hash mismatch: $payloadHash"
}

function Invoke-PackageCommand {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string[]]$Arguments
  )

  & $Executable @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "$(Split-Path -Leaf $Executable) $($Arguments -join ' ') failed"
  }
}

Invoke-PackageCommand `
  -Executable (Join-Path $resolvedRoot 'cyan.exe') -Arguments '--version'
Invoke-PackageCommand `
  -Executable (Join-Path $resolvedRoot 'cgen.exe') -Arguments '--help'
Invoke-PackageCommand `
  -Executable (Join-Path $resolvedRoot 'ipapatch.exe') -Arguments '--version'

$previousQpaPlatform =
  [Environment]::GetEnvironmentVariable('QT_QPA_PLATFORM', 'Process')
$previousRhiBackend =
  [Environment]::GetEnvironmentVariable('QSG_RHI_BACKEND', 'Process')
$guiProcess = $null

try {
  [Environment]::SetEnvironmentVariable('QT_QPA_PLATFORM', 'offscreen', 'Process')
  [Environment]::SetEnvironmentVariable('QSG_RHI_BACKEND', 'software', 'Process')

  $startInfo = New-Object System.Diagnostics.ProcessStartInfo
  $startInfo.FileName = Join-Path $resolvedRoot 'cyan-gui.exe'
  $startInfo.WorkingDirectory = $resolvedRoot
  $startInfo.UseShellExecute = $false
  $startInfo.CreateNoWindow = $true

  $guiProcess = [System.Diagnostics.Process]::Start($startInfo)
  if ($null -eq $guiProcess) {
    throw 'cyan-gui.exe could not be started'
  }

  if ($guiProcess.WaitForExit($GuiStartupSeconds * 1000)) {
    throw "cyan-gui.exe exited during startup with code $($guiProcess.ExitCode)"
  }
}
finally {
  if ($null -ne $guiProcess -and -not $guiProcess.HasExited) {
    $guiProcess.Kill()
    $guiProcess.WaitForExit()
  }
  [Environment]::SetEnvironmentVariable(
    'QT_QPA_PLATFORM',
    $previousQpaPlatform,
    'Process'
  )
  [Environment]::SetEnvironmentVariable(
    'QSG_RHI_BACKEND',
    $previousRhiBackend,
    'Process'
  )
}

Write-Host "Verified packaged CLI tools and GUI startup in $resolvedRoot"
