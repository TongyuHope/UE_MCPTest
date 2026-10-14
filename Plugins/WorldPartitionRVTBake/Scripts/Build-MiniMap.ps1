# Produces the World Partition editor preview, not a packaged game's HUD minimap.
[CmdletBinding()]
param(
    [string]$Editor = 'E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$Project = 'D:\UEProjectReal\UE_MCPTest\UE_MCPTest.uproject',
    [string]$Map = '/Game/Level/BakeTest',
    [switch]$DebugCapture,
    [switch]$GeneratePCG,
    [string[]]$PCGGraphs = @(),
    [ValidateRange(1, 2147483647)][int]$PCGCellSize = 25600,
    [string]$SourceControlProvider = 'None',
    [string[]]$EnablePlugins = @(),
    [switch]$EnablePython,
    [switch]$Preview
)
$ErrorActionPreference = 'Stop'
$builderArguments = @()
if ($DebugCapture) { $builderArguments += '-DebugCapture' }
& (Join-Path $PSScriptRoot 'Invoke-WorldPartitionBuild.ps1') `
    -Editor $Editor -Project $Project -Map $Map `
    -Builder 'WorldPartitionMiniMapBuilder' -BuilderArguments $builderArguments `
    -GeneratePCG:$GeneratePCG -PCGGraphs $PCGGraphs -PCGCellSize $PCGCellSize `
    -SourceControlProvider $SourceControlProvider -EnablePlugins $EnablePlugins `
    -EnablePython:$EnablePython -Preview:$Preview
