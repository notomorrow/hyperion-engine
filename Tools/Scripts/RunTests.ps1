#Requires -Version 7.0
param(
    [Parameter(Position = 0)][string]$Filter,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ExtraArgs
)

# Runs every test project under Tests/ (any .csproj with <IsTestProject>true</IsTestProject>).
#
#   ./Tools/Scripts/RunTests.ps1                     run all tests
#   ./Tools/Scripts/RunTests.ps1 Native              run tests whose full name contains "Native" (class, method or namespace)
#   ./Tools/Scripts/RunTests.ps1 "Name=SavedAssets_LoadInNewSession"
#                                                    a filter containing = ~ ! | & is passed to dotnet test --filter as is
#   ./Tools/Scripts/RunTests.ps1 Native -c Release   anything after the filter goes to dotnet test (use "" for no filter)

$ErrorActionPreference = "Stop"

$testsDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".." ".." "Tests"))

if ($env:MSBuildSdksPath -and -not (Test-Path $env:MSBuildSdksPath))
{
    Write-Host "Ignoring MSBuildSdksPath ($env:MSBuildSdksPath doesn't exist)" -ForegroundColor Yellow
    $env:MSBuildSdksPath = $null
}

$projects = Get-ChildItem $testsDir -Recurse -File -Filter *.csproj |
    Where-Object { $_.FullName -notmatch '[\\/](bin|obj|Fixtures)[\\/]' } |
    Where-Object { Select-String -Path $_.FullName -Pattern '<IsTestProject>\s*true\s*</IsTestProject>' -Quiet } |
    Sort-Object FullName

if (-not $projects)
{
    Write-Error "No test projects found under $testsDir"
}

$testArgs = @()

if ($Filter)
{
    if ($Filter -notmatch '[=~!|&]')
    {
        $Filter = "FullyQualifiedName~$Filter"
    }

    $testArgs += @("--filter", $Filter)
}

if ($ExtraArgs)
{
    $testArgs += $ExtraArgs
}

$failed = @()

foreach ($project in $projects)
{
    Write-Host "==> $([System.IO.Path]::GetRelativePath($testsDir, $project.FullName))" -ForegroundColor Cyan

    & dotnet test $project.FullName @testArgs

    if ($LASTEXITCODE -ne 0)
    {
        $failed += $project.Name
    }
}

if ($failed)
{
    Write-Host "Failed: $($failed -join ', ')" -ForegroundColor Red
    exit 1
}

Write-Host "All test projects passed" -ForegroundColor Green
