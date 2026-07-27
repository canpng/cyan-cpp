[CmdletBinding()]
param(
  [Parameter(Mandatory)]
  [string]$CorpusDirectory,

  [Parameter(Mandatory)]
  [string]$CyanExecutable,

  [Parameter(Mandatory)]
  [string]$ReferenceIpaPatchExecutable,

  [Parameter(Mandatory)]
  [string]$CppIpaPatchExecutable,

  [string]$Dylib,
  [string[]]$CyanArguments = @(),
  [ValidateRange(1, 20)]
  [int]$Repetitions = 3,
  [string]$OutputCsv = "ipapatch-benchmark.csv"
)

$ErrorActionPreference = "Stop"

function Resolve-File([string]$Path, [string]$Label) {
  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
  if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
    throw "$Label is not a file: $Path"
  }
  return $resolved.Path
}

function Invoke-MeasuredProcess([string]$Executable, [string[]]$Arguments) {
  $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
  $startInfo.FileName = $Executable
  $startInfo.UseShellExecute = $false
  $startInfo.RedirectStandardOutput = $true
  $startInfo.RedirectStandardError = $true
  foreach ($argument in $Arguments) {
    [void]$startInfo.ArgumentList.Add($argument)
  }

  $process = [System.Diagnostics.Process]::new()
  $process.StartInfo = $startInfo
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  [void]$process.Start()
  $stdoutTask = $process.StandardOutput.ReadToEndAsync()
  $stderrTask = $process.StandardError.ReadToEndAsync()
  $process.WaitForExit()
  $timer.Stop()
  $stdout = $stdoutTask.GetAwaiter().GetResult()
  $stderr = $stderrTask.GetAwaiter().GetResult()
  if ($process.ExitCode -ne 0) {
    throw "Process failed ($($process.ExitCode)): $Executable $($Arguments -join ' ')`n$stdout`n$stderr"
  }

  [pscustomobject]@{
    WallMilliseconds = $timer.Elapsed.TotalMilliseconds
    CpuMilliseconds = $process.TotalProcessorTime.TotalMilliseconds
    PeakBytes = $process.PeakWorkingSet64
    Log = $stdout + $stderr
  }
}

function Get-SizeClass([long]$Bytes) {
  if ($Bytes -lt 500MB) { return "small" }
  if ($Bytes -lt 2GB) { return "medium" }
  return "large"
}

$cyan = Resolve-File $CyanExecutable "cyan"
$reference = Resolve-File $ReferenceIpaPatchExecutable "reference ipapatch"
$cpp = Resolve-File $CppIpaPatchExecutable "C++ ipapatch"
$corpus = (Resolve-Path -LiteralPath $CorpusDirectory -ErrorAction Stop).Path
$payload = if ($Dylib) { Resolve-File $Dylib "payload dylib" } else { $null }
$inputs = @(Get-ChildItem -LiteralPath $corpus -File |
    Where-Object { $_.Extension -in @(".ipa", ".tipa") } |
    Sort-Object Name)
if ($inputs.Count -eq 0) {
  throw "No .ipa or .tipa files were found in $corpus"
}

$scratch = Join-Path ([System.IO.Path]::GetTempPath()) ("cyan-ipapatch-benchmark-" + [guid]::NewGuid())
[void](New-Item -ItemType Directory -Path $scratch)
$rows = [System.Collections.Generic.List[object]]::new()

try {
  foreach ($input in $inputs) {
    for ($iteration = 1; $iteration -le $Repetitions; $iteration++) {
      $caseDirectory = Join-Path $scratch "$($input.BaseName)-$iteration"
      [void](New-Item -ItemType Directory -Path $caseDirectory)

      $legacyIntermediate = Join-Path $caseDirectory "legacy-cyan.ipa"
      $legacyOutput = Join-Path $caseDirectory "legacy-reference.ipa"
      $integratedOutput = Join-Path $caseDirectory "integrated.ipa"
      $standaloneOutput = Join-Path $caseDirectory "standalone.ipa"

      $cyanBase = @("--input", $input.FullName, "--overwrite") + $CyanArguments
      $legacyCyan = Invoke-MeasuredProcess $cyan ($cyanBase + @("--output", $legacyIntermediate))
      $referenceArgs = @("--input", $legacyIntermediate, "--output", $legacyOutput, "--noconfirm")
      if ($payload) { $referenceArgs += @("--dylib", $payload) }
      $legacyPatch = Invoke-MeasuredProcess $reference $referenceArgs

      $integratedArgs = $cyanBase + @("--ipapatch", "--output", $integratedOutput)
      if ($payload) { $integratedArgs += @("--ipapatch-dylib", $payload) }
      $integrated = Invoke-MeasuredProcess $cyan $integratedArgs

      $standaloneArgs = @("--input", $input.FullName, "--output", $standaloneOutput, "--noconfirm")
      if ($payload) { $standaloneArgs += @("--dylib", $payload) }
      $standalone = Invoke-MeasuredProcess $cpp $standaloneArgs

      $common = @{
        Input = $input.Name
        SizeClass = Get-SizeClass $input.Length
        InputBytes = $input.Length
        Iteration = $iteration
      }
      $rows.Add([pscustomobject]($common + @{
        Flow = "cyan-then-reference-ipapatch"
        WallMilliseconds = $legacyCyan.WallMilliseconds + $legacyPatch.WallMilliseconds
        CpuMilliseconds = $legacyCyan.CpuMilliseconds + $legacyPatch.CpuMilliseconds
        PeakBytes = [Math]::Max($legacyCyan.PeakBytes, $legacyPatch.PeakBytes)
        OutputBytes = (Get-Item -LiteralPath $legacyOutput).Length
        ExtractCount = ([regex]::Matches($legacyCyan.Log, "extracting ipa")).Count
        PackageCount = ([regex]::Matches($legacyCyan.Log, "generating ipa")).Count
      }))
      $rows.Add([pscustomobject]($common + @{
        Flow = "integrated-cyan-ipapatch"
        WallMilliseconds = $integrated.WallMilliseconds
        CpuMilliseconds = $integrated.CpuMilliseconds
        PeakBytes = $integrated.PeakBytes
        OutputBytes = (Get-Item -LiteralPath $integratedOutput).Length
        ExtractCount = ([regex]::Matches($integrated.Log, "extracting ipa")).Count
        PackageCount = ([regex]::Matches($integrated.Log, "generating ipa")).Count
      }))
      $rows.Add([pscustomobject]($common + @{
        Flow = "standalone-cpp-ipapatch"
        WallMilliseconds = $standalone.WallMilliseconds
        CpuMilliseconds = $standalone.CpuMilliseconds
        PeakBytes = $standalone.PeakBytes
        OutputBytes = (Get-Item -LiteralPath $standaloneOutput).Length
        ExtractCount = ([regex]::Matches($standalone.Log, "\[\*\] extracting:")).Count
        PackageCount = ([regex]::Matches($standalone.Log, "\[\*\] packaging:")).Count
      }))
    }
  }

  $destination = [System.IO.Path]::GetFullPath($OutputCsv)
  $rows | Export-Csv -LiteralPath $destination -NoTypeInformation -Encoding utf8
  Write-Host "Benchmark results: $destination"
} finally {
  if ($scratch.StartsWith([System.IO.Path]::GetTempPath(), [System.StringComparison]::OrdinalIgnoreCase)) {
    Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
  }
}
