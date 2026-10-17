param(
    [Parameter(Mandatory = $true)][string]$Project,
    [switch]$Run,
    [switch]$ServeOnly,
    [switch]$FastLink,
    [int]$MaxTextureSize = 0,
    [string]$Worlds = "",
    [int]$Port = 8137
)

# Builds a project for the browser into <project>/Binaries/Web, a folder any static host can serve as it is.
# Intermediates go to <project>/Build/Web. Settings come from the project's PlatformSettings.Web, the arguments override.
#
#   -Run        serve Binaries/Web on localhost afterwards and open it in the browser
#   -ServeOnly  skip the build: serve the last one and open it in the browser
#   -FastLink   link at -O1: minutes faster, for testing locally; a build to host links at -O3
#
# Build/Web/Built.stamp is written by a finished build, dated when that build started.

$ErrorActionPreference = "Stop"

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$webSdk = Join-Path $rootDir "Binaries\Web\Release\Sdk\CMake\HyperionWebSdk.cmake"
$templateDir = Join-Path $rootDir "Source\Templates\Platforms\Web"
$webConfig = Join-Path $rootDir "Config\EngineConfig.Web.json"
$pinnedEmscriptenVersion = "6.0.11"

function Step([int]$percent, [string]$text)
{
    Write-Host "==> [$percent%] $text"
}

function Fail([string]$message)
{
    Write-Host "ERROR: $message"
    exit 1
}

function Invoke-Cmd([string]$commandLine, [string]$failure)
{
    # through a batch file: quotes inside the command survive it, not PowerShell's handing of them to cmd
    $stepScript = Join-Path $buildDir "Step.bat"
    Set-Content $stepScript "@echo off`r`n$commandLine`r`nexit /b %errorlevel%" -Encoding ascii

    & cmd /c "`"$stepScript`""

    if ($LASTEXITCODE -ne 0)
    {
        Fail $failure
    }
}

# Start-Process joins its arguments with spaces, so one holding a space needs its own quotes
function Format-Arguments([string[]]$arguments)
{
    return $arguments | ForEach-Object { if ($_ -match "\s" -and -not $_.StartsWith('"')) { "`"$_`"" } else { $_ } }
}

# the project

if (-not (Test-Path $Project))
{
    Fail "Project folder $Project not found"
}

$Project = (Resolve-Path $Project).Path
$projectFile = Get-ChildItem $Project -Filter *.hypproject | Select-Object -First 1

if (-not $projectFile)
{
    Fail "No .hypproject in $Project"
}

# the same name the editor gives a project's game code
$name = ($projectFile.BaseName -replace '[^A-Za-z0-9_]', '') -replace '^[0-9]+', ''

if ($name -eq "")
{
    $name = "MyGame"
}

if (Test-Path (Join-Path $Project "Source\$name.csproj"))
{
    Fail "C# projects can't be built for the web yet"
}

$projectText = Get-Content $projectFile.FullName -Raw

function Get-ProjectSetting([string]$key, [string]$fallback)
{
    $match = [regex]::Match($projectText, "(?s)PlatformSettings.*?\b$key\s*=\s*""?([^""\r\n}]*)""?")

    if ($match.Success -and $match.Groups[1].Value.Trim() -ne "")
    {
        return $match.Groups[1].Value.Trim()
    }

    return $fallback
}

$editWorld = [regex]::Match($projectText, "EditWorldName\s*=\s*(\S+)").Groups[1].Value

if ($MaxTextureSize -le 0) { $MaxTextureSize = [int](Get-ProjectSetting "MaxTextureSize" "512") }
if ($Worlds -eq "") { $Worlds = Get-ProjectSetting "Worlds" $editWorld }

$worldList = @($Worlds -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
$gameSourceDir = Join-Path $Project "Source"
$hasGameCode = Test-Path (Join-Path $gameSourceDir "Game")

$buildDir = Join-Path $Project "Build\Web"
$packageDir = Join-Path $buildDir "Package"
$cmakeDir = Join-Path $buildDir "CMake"
$pageDir = Join-Path $buildDir "Page"
$outputDir = Join-Path $Project "Binaries\Web"
$buildingStamp = Join-Path $buildDir "Building.stamp"
$builtStamp = Join-Path $buildDir "Built.stamp"

function Start-WebServer
{
    if (-not (Get-Command python -ErrorAction SilentlyContinue))
    {
        Fail "python not found on PATH, which serving the page needs"
    }

    $serverPidFile = Join-Path $buildDir "server.pid"

    if (Test-Path $serverPidFile)
    {
        $previousServer = Get-Process -Id ([int](Get-Content $serverPidFile -TotalCount 1)) -ErrorAction SilentlyContinue

        if ($previousServer -and $previousServer.ProcessName -match "python")
        {
            Stop-Process -Id $previousServer.Id -Force
        }
    }

    $server = Start-Process -FilePath python -ArgumentList @("`"$(Join-Path $PSScriptRoot 'ServeWeb.py')`"", "`"$outputDir`"", "`"$outputDir`"", $Port, "`"$(Join-Path $buildDir 'MissingShaders.txt')`"") -WindowStyle Hidden -PassThru
    Set-Content $serverPidFile $server.Id -Encoding ascii

    Start-Sleep -Milliseconds 800
    Start-Process "http://localhost:$Port/"

    Write-Host "Serving $outputDir at http://localhost:$Port/ (server pid $($server.Id))"
}

New-Item -ItemType Directory -Force $buildDir | Out-Null

if ($ServeOnly)
{
    if (-not (Test-Path (Join-Path $outputDir "index.html")) -or -not (Test-Path (Join-Path $outputDir "$name.wasm")))
    {
        Fail "$name hasn't been built for the web yet"
    }

    Start-WebServer
    exit 0
}

Set-Content $buildingStamp "" -Encoding ascii

Write-Host "Building $name for the web: worlds $($worldList -join ', '), textures up to $MaxTextureSize"

# 1. the toolchain

Step 0 "Checking the toolchain"

if (-not $env:EMSDK -or -not (Test-Path (Join-Path $env:EMSDK "emsdk_env.bat")))
{
    Fail "EMSDK is not set to an emsdk folder. Install emsdk $pinnedEmscriptenVersion and set EMSDK to it"
}

$emscriptenVersionFile = Join-Path $env:EMSDK "upstream\emscripten\emscripten-version.txt"

if ((Test-Path $emscriptenVersionFile) -and -not ((Get-Content $emscriptenVersionFile -Raw) -match [regex]::Escape($pinnedEmscriptenVersion)))
{
    Write-Host "WARNING: emsdk is not $pinnedEmscriptenVersion, the version the engine's web build is made with"
}

if (-not (Get-Command ninja -ErrorAction SilentlyContinue))
{
    Fail "ninja not found on PATH"
}

if ($Run -and -not (Get-Command python -ErrorAction SilentlyContinue))
{
    Fail "python not found on PATH, which serving the page needs"
}

if (-not (Test-Path $webSdk))
{
    Fail "The engine's web build is missing ($webSdk). Build it: Tools\Scripts\BuildHyperion.bat web"
}

$cookDir = Join-Path $rootDir "Binaries\Windows\Release"
$cookExe = Join-Path $cookDir "BlobStorageCookCommandlet.exe"

if (-not (Test-Path $cookExe))
{
    Fail "The editor build's cook commandlet is missing ($cookExe). Build the engine: Tools\Scripts\BuildHyperion.bat"
}

# 2. the cook: what the worlds reach, the engine defaults, and the shaders they draw with on the web

Step 4 "Cooking and compiling shaders"

if (Test-Path $packageDir)
{
    Remove-Item $packageDir -Recurse -Force
}

New-Item -ItemType Directory -Force $packageDir | Out-Null

# without one, the cook would take the shader lists the host tools' own cache holds
$emptyShaderCache = Join-Path $buildDir "NoShaderLists"
New-Item -ItemType Directory -Force $emptyShaderCache | Out-Null

$cookArguments = @("--project=$Project", "--out-cache=$packageDir\Cache", "--out-content=$packageDir\Content", "--max-texture-size=$MaxTextureSize",
    "--shader-target=web", "--target-config=$webConfig", "--cachedir=$emptyShaderCache")

if ($worldList.Count -gt 0)
{
    $cookArguments += "--worlds=$($worldList -join ',')"
}

$cookLog = Join-Path $buildDir "Cook.log"
$cookErrorLog = Join-Path $buildDir "Cook.err.log"

Start-Process -FilePath $cookExe -ArgumentList (Format-Arguments $cookArguments) -WorkingDirectory $cookDir -Wait -NoNewWindow `
    -RedirectStandardOutput $cookLog -RedirectStandardError $cookErrorLog | Out-Null

# the commandlet can crash shutting down after a finished cook, so the log decides
if (-not (Select-String -Path $cookLog, $cookErrorLog -Pattern "Blob storage cook complete." -Quiet))
{
    Fail "The cook failed, see $cookLog"
}

$configDir = Join-Path $packageDir "Config"
New-Item -ItemType Directory -Force $configDir | Out-Null
Copy-Item $webConfig (Join-Path $configDir "EngineConfig.json")
Copy-Item (Join-Path $rootDir "Config\GlobalConfig.Web.json") (Join-Path $configDir "GlobalConfig.json")
Copy-Item (Join-Path $rootDir "Config\Shaders.hmf"), (Join-Path $rootDir "Config\DeviceTiers.hmf") $configDir

# 3. the page: the launch layer and the project's game code, linked against the engine's web build

Step 40 "Building the page"

if (Test-Path $pageDir)
{
    # the package is preloaded at link time, which only runs again when its output is gone
    Remove-Item (Join-Path $pageDir "$name.js") -ErrorAction SilentlyContinue
}

$gameDirArgument = if ($hasGameCode) { $gameSourceDir -replace '\\', '/' } else { "" }
$linkOptimization = if ($FastLink) { "-O1" } else { "-O3" }

Invoke-Cmd ("set `"EMSDK_QUIET=1`" && call `"$env:EMSDK\emsdk_env.bat`" >nul && emcmake cmake -S `"$templateDir`" -B `"$cmakeDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release " + `
    "-DHYPERION_WEB_SDK=`"$($webSdk -replace '\\', '/')`" -DHYP_WEB_PAGE_NAME=$name -DHYP_WEB_PACKAGE=`"$($packageDir -replace '\\', '/')`" " + `
    "-DHYP_WEB_GAME_DIR=`"$gameDirArgument`" -DHYP_WEB_OUTPUT=`"$($pageDir -replace '\\', '/')`" -DHYP_WEB_LINK_OPTIMIZATION=$linkOptimization && cmake --build `"$cmakeDir`"") `
    "Building the page failed"

# 4. stage everything a host needs into Binaries/Web, leaving anything else the folder holds (a CNAME, say) alone

Step 97 "Staging into $outputDir"

New-Item -ItemType Directory -Force $outputDir | Out-Null

$pageFiles = @("index.html", "isolation-worker.js", "$name.js", "$name.wasm", "$name.data")

foreach ($fileName in $pageFiles)
{
    $source = Join-Path $pageDir $fileName

    if (-not (Test-Path $source))
    {
        Fail "$source not found after building the page"
    }

    Copy-Item $source $outputDir -Force
}

$outputCacheDir = Join-Path $outputDir "Cache"

if (Test-Path $outputCacheDir)
{
    Remove-Item $outputCacheDir -Recurse -Force
}

Copy-Item (Join-Path $packageDir "Cache") $outputCacheDir -Recurse

# a static host has no directory listing, so the page reads the names from here
(Get-ChildItem $outputCacheDir -File | Where-Object { $_.Name -ne "index.txt" } | ForEach-Object { $_.Name }) | Set-Content (Join-Path $outputCacheDir "index.txt") -Encoding ascii

# GitHub Pages runs a site through Jekyll unless this is there
Set-Content (Join-Path $outputDir ".nojekyll") "" -Encoding ascii

foreach ($file in Get-ChildItem $outputDir -Recurse -File)
{
    if ($file.Length -gt 100MB)
    {
        Write-Host "WARNING: $($file.Name) is over 100 MiB, more than GitHub allows in one file"
    }
}

$totalBytes = (Get-ChildItem $outputDir -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host ("Built {0} for the web: {1} ({2:N0} MB)" -f $name, $outputDir, ($totalBytes / 1MB))

# moving keeps the time the build started, so anything saved during the build still counts as newer
Move-Item $buildingStamp $builtStamp -Force

# 5. serve it, when asked

if ($Run)
{
    Start-WebServer
}

exit 0
