# Calls UE's official builder; HLOD Layer and actor settings stay in the map.
[CmdletBinding()]
param(
    [string]$Editor = 'E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$Project = 'D:\UEProjectReal\UE_MCPTest\UE_MCPTest.uproject',
    [string]$Map = '/Game/Level/BakeTest',
    [ValidateSet('SetupBuild', 'Setup', 'Build', 'Rebuild')]
    [string]$Action = 'SetupBuild',
    [string]$HLODLayer = '',
    [switch]$GeneratePCG,
    [string[]]$PCGGraphs = @(),
    [ValidateRange(1, 2147483647)][int]$PCGCellSize = 25600,
    [string]$SourceControlProvider = 'None',
    [string[]]$EnablePlugins = @(),
    [switch]$EnablePython,
    [switch]$Preview
)
$ErrorActionPreference = 'Stop'
if ($HLODLayer -and ($HLODLayer -match '[/\\.;\s]')) {
    throw 'HLODLayer must be an asset name, for example HLODLayer_Default, not an asset path.'
}
if ($HLODLayer -and $Action -eq 'Setup') {
    throw 'HLODLayer filters the build phase, so it cannot be used with Setup alone.'
}
$builderArguments = switch ($Action) {
    'SetupBuild' { '-SetupHLODs'; '-BuildHLODs' }
    'Setup' { '-SetupHLODs' }
    'Build' { '-BuildHLODs' }
    'Rebuild' { '-SetupHLODs'; '-RebuildHLODs' }
}
if ($HLODLayer) { $builderArguments = @($builderArguments) + "-BuildHLODLayer=$HLODLayer" }
& (Join-Path $PSScriptRoot 'Invoke-WorldPartitionBuild.ps1') `
    -Editor $Editor -Project $Project -Map $Map `
    -Builder 'WorldPartitionHLODsBuilder' -BuilderArguments @($builderArguments) `
    -GeneratePCG:$GeneratePCG -PCGGraphs $PCGGraphs -PCGCellSize $PCGCellSize `
    -SourceControlProvider $SourceControlProvider -EnablePlugins $EnablePlugins `
    -EnablePython:$EnablePython -Preview:$Preview
