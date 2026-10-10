[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$pluginSource = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pluginTarget = 'D:\UEProjectReal\UE_MCPTest\Plugins\WorldPartitionRVTBake'
$backupRoot = Join-Path 'D:\UEProjectReal\UE_MCPTest\Saved\WorldPartitionRVTBakeBackups' (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$codeItems = @('Source', 'Resources', 'Scripts', 'README.md', 'TEST_RESULTS.md', 'WorldPartitionRVTBake.uplugin')
if ($pluginSource.Equals($pluginTarget, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Run this synchronization script from the ZDG source plugin, not from the test copy.'
}
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
New-Item -ItemType Directory -Path $pluginTarget -Force | Out-Null
foreach ($codeItem in $codeItems) {
    $existingItem = Join-Path $pluginTarget $codeItem
    if (Test-Path -LiteralPath $existingItem) {
        Copy-Item -LiteralPath $existingItem -Destination $backupRoot -Recurse -Force
    }
    $sourceItem = Join-Path $pluginSource $codeItem
    if (Test-Path -LiteralPath $sourceItem) {
        Copy-Item -LiteralPath $sourceItem -Destination $pluginTarget -Recurse -Force
    }
}
foreach ($sourceFile in Get-ChildItem -LiteralPath (Join-Path $pluginSource 'Source') -Recurse -File) {
    $relativeFile = [System.IO.Path]::GetRelativePath($pluginSource, $sourceFile.FullName)
    $targetFile = Join-Path $pluginTarget $relativeFile
    $sourceHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash
    $targetHash = (Get-FileHash -LiteralPath $targetFile -Algorithm SHA256).Hash
    if ($sourceHash -ne $targetHash) { throw "Plugin sync hash mismatch: $relativeFile" }
}
Write-Output "Plugin code synchronized and verified: $pluginTarget"
Write-Output "Previous plugin source backup: $backupRoot"
