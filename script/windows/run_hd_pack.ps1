param(
    [ValidateSet(1, 2, 3, 4)][int]$Scale = 2,
    [ValidateSet('pixel', 'smooth')][string]$Style = 'pixel',
    [string]$DataRoot,
    [string]$PackRoot,
    [string]$ExecutablePath
)
$ErrorActionPreference = 'Stop'
$taskRepository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $DataRoot) { $DataRoot = Join-Path $taskRepository '../..' }
$DataRoot = (Resolve-Path -LiteralPath $DataRoot).Path
if (-not $PackRoot) { $PackRoot = Join-Path $DataRoot "upscaled/$Style" }
if (-not $ExecutablePath) { $ExecutablePath = Join-Path $taskRepository 'build-hd/fheroes2.exe' }
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw "Build fheroes2 with ENABLE_IMAGE=ON first, or supply -ExecutablePath." }
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot 'data/Heroes2.agg') -PathType Leaf)) { throw "DataRoot must contain data/Heroes2.agg." }
if ($Scale -gt 1 -and -not (Test-Path -LiteralPath (Join-Path $PackRoot "x$Scale/graphics") -PathType Container)) { throw "Missing graphics pack: $PackRoot/x$Scale/graphics" }
$taskProfile = Join-Path $taskRepository 'runtime-hd/profile'
$taskConfigDirectory = Join-Path $taskProfile 'fheroes2'
New-Item -ItemType Directory -Force -Path $taskConfigDirectory | Out-Null
$taskConfigPath = Join-Path $taskConfigDirectory 'fheroes2.cfg'
$taskConfig = if (Test-Path -LiteralPath $taskConfigPath) { Get-Content -LiteralPath $taskConfigPath -Raw } else { "screen scaling type = nearest`n" }
$taskOptions = @{ 'graphics pack' = $PackRoot.Replace('\', '/'); 'graphics scale' = $Scale.ToString() }
foreach ($taskKey in $taskOptions.Keys) {
    $taskPattern = '(?m)^' + [regex]::Escape($taskKey) + '\s*=.*$'
    $taskLine = "$taskKey = $($taskOptions[$taskKey])"
    if ([regex]::IsMatch($taskConfig, $taskPattern)) {
        $taskReplacement = $taskLine.Replace('$', '$$')
        $taskConfig = [regex]::Replace($taskConfig, $taskPattern, $taskReplacement)
    } else { $taskConfig += "`n$taskLine`n" }
}
[System.IO.File]::WriteAllText($taskConfigPath, $taskConfig, (New-Object System.Text.UTF8Encoding($false)))
$taskStart = New-Object System.Diagnostics.ProcessStartInfo
$taskStart.FileName = (Resolve-Path -LiteralPath $ExecutablePath).Path
$taskStart.WorkingDirectory = Split-Path -Parent $taskStart.FileName
$taskStart.UseShellExecute = $false
$taskStart.CreateNoWindow = $true
$taskStart.EnvironmentVariables['FHEROES2_DATA'] = $DataRoot
$taskStart.EnvironmentVariables['APPDATA'] = $taskProfile
# Scope profile selection to this child process; do not change the user's shell.
$taskStart.EnvironmentVariables.Remove('HOME')
$taskProcess = [System.Diagnostics.Process]::Start($taskStart)
$taskProcess.WaitForExit()
exit $taskProcess.ExitCode
