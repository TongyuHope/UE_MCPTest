# Shared launcher. Use Build-HLOD.ps1 / Build-MiniMap.ps1 as the public entry points.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Editor,
    [Parameter(Mandatory)][string]$Project,
    [Parameter(Mandatory)][string]$Map,
    [Parameter(Mandatory)]
    [ValidateSet('WorldPartitionHLODsBuilder', 'WorldPartitionMiniMapBuilder')]
    [string]$Builder,
    [string[]]$BuilderArguments = @(),
    [switch]$GeneratePCG,
    [string[]]$PCGGraphs = @(),
    [ValidateRange(1, 2147483647)][int]$PCGCellSize = 25600,
    [string]$SourceControlProvider = 'None',
    [string[]]$EnablePlugins = @(),
    [switch]$EnablePython,
    [switch]$Preview
)
$ErrorActionPreference = 'Stop'
if ($PCGGraphs.Count -and !$GeneratePCG) {
    throw 'PCGGraphs requires GeneratePCG. Otherwise no PCG prepass would run.'
}
foreach ($graph in $PCGGraphs) {
    if ([string]::IsNullOrWhiteSpace($graph) -or $graph -match '[/\\.;\s]') {
        throw 'PCGGraphs must contain graph asset names, not paths or a joined list.'
    }
}
foreach ($plugin in $EnablePlugins) {
    if ([string]::IsNullOrWhiteSpace($plugin) -or $plugin -match '[,;\s]') {
        throw 'EnablePlugins must contain individual plugin names.'
    }
}
if (!(Test-Path -LiteralPath $Editor -PathType Leaf)) { throw "Editor not found: $Editor" }
if (!(Test-Path -LiteralPath $Project -PathType Leaf) -or [IO.Path]::GetExtension($Project) -ne '.uproject') {
    throw "Project file not found or not a .uproject: $Project"
}
$Editor = (Resolve-Path -LiteralPath $Editor).Path
$Project = (Resolve-Path -LiteralPath $Project).Path
$projectRoot = Split-Path -Parent $Project
if ($Map -notmatch '^/Game/[^\s\\.:*?"<>|]+$' -or $Map.EndsWith('/')) {
    throw 'Map must be a /Game/... package path without .umap, for example /Game/Level/BakeTest.'
}
$mapFile = Join-Path (Join-Path $projectRoot 'Content') ($Map.Substring(6) + '.umap')
if (!(Test-Path -LiteralPath $mapFile -PathType Leaf)) { throw "Map file not found: $mapFile" }

$commonArguments = @($Project, $Map, '-run=WorldPartitionBuilderCommandlet',
    '-AllowCommandletRendering', '-Unattended', '-stdout', '-FullStdOutLogOutput')
if ($SourceControlProvider -ne 'Default') {
    if ([string]::IsNullOrWhiteSpace($SourceControlProvider)) { throw 'SourceControlProvider cannot be empty.' }
    $commonArguments += "-SCCProvider=$SourceControlProvider"
}
if (!$EnablePython) { $commonArguments += '-DisablePython' }
if ($EnablePlugins.Count) { $commonArguments += '-EnablePlugins=' + ($EnablePlugins -join ',') }
$logDirectory = Join-Path $projectRoot 'Saved\Logs\WorldPartitionBuild'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$mapName = ($Map -split '/')[-1]
$phases = @()
if ($GeneratePCG) {
    $pcgArguments = @('-IterativeCellLoading', "-IterativeCellSize=$PCGCellSize", '-GenerateComponentEditingModeNormal')
    if ($PCGGraphs.Count) { $pcgArguments += '-IncludeGraphNames=' + ($PCGGraphs -join ';') }
    $phases += [pscustomobject]@{ Builder = 'PCGWorldPartitionBuilder'; Arguments = $pcgArguments }
}
$phases += [pscustomobject]@{ Builder = $Builder; Arguments = $BuilderArguments }
foreach ($phase in $phases) {
    $logPath = Join-Path $logDirectory "$mapName-$($phase.Builder)-$stamp.log"
    $arguments = $commonArguments + "-Builder=$($phase.Builder)" + @($phase.Arguments) + "-abslog=$logPath"
    if ($Preview) {
        # These are inspectable arguments, not shell text to execute via Invoke-Expression.
        [pscustomobject]@{ Phase = $phase.Builder; Editor = $Editor; Arguments = $arguments; LogPath = $logPath }
        continue
    }
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    Write-Host "Running $($phase.Builder) on $Map"
    Write-Host "Log: $logPath"
    & $Editor @arguments
    $phaseExitCode = $LASTEXITCODE
    if ($phaseExitCode -ne 0) {
        throw "$($phase.Builder) exited with code $phaseExitCode. Later phases were not run. Inspect $logPath; startup errors can also cause a nonzero exit."
    }
    Write-Host "Process completed: $($phase.Builder). Check its log and generated assets to verify the result."
}
