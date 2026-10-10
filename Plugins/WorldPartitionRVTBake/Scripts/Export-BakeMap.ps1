[CmdletBinding()]
param(
    [string]$Editor = 'E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$Project = 'D:\UEProjectReal\UE_MCPTest\UE_MCPTest.uproject',
    [string]$Map = '/Game/Level/BakeTest',
    [string]$NewPackage = '/Game/Level/BakeTest_RVTBake',
    [string]$PCGActor = 'BakeTest',
    [string]$RVTVolume = 'RuntimeVirtualTextureVolume_UAID_047C160156AB250903_2076114856',
    [string[]]$IncludeActors = @(),
    [string[]]$IncludeActorTags = @(),
    [switch]$ReportOnly
)
$ErrorActionPreference = 'Stop'
$exportArguments = @(
    $Project, $Map,
    '-run=WorldPartitionBuilderCommandlet',
    '-Builder=WorldPartitionRVTBakeMapBuilder',
    "-NewPackage=$NewPackage", "-PCGActor=$PCGActor", "-RVTVolume=$RVTVolume",
    '-SCCProvider=None', '-Unattended', '-stdout', '-FullStdOutLogOutput'
)
if ($IncludeActors.Count) { $exportArguments += '-IncludeActors=' + ($IncludeActors -join ',') }
if ($IncludeActorTags.Count) { $exportArguments += '-IncludeActorTags=' + ($IncludeActorTags -join ',') }
if ($ReportOnly) { $exportArguments += '-ReportOnly' }
& $Editor @exportArguments
if ($LASTEXITCODE -ne 0) { throw "Bake map export failed with exit code $LASTEXITCODE. Inspect the project log and Saved/RVTBakeExport." }
