param(
    [Parameter(Mandatory = $true)][string]$Project,
    [switch]$Run,
    [switch]$FastLink,
    [switch]$Record,
    [switch]$NoRecord,
    [int]$MaxTextureSize = 0,
    [string]$Worlds = "",
    [int]$RecordSeconds = 0,
    [int]$FrameRate = 0,
    [int]$Port = 8137
)

# Builds a project for the browser into <project>/Binaries/Web, a folder any static host can serve as it is.
# Intermediates go to <project>/Build/Web. Settings come from the project's PlatformSettings.Web, the arguments override.
#
#   -Run       serve Binaries/Web on localhost afterwards and open it in the browser
#   -Record    record the level's engine assets and shaders again even when the recorded lists look current
#   -NoRecord  use whatever lists there are
#   -FastLink  link at -O1: minutes faster, for testing locally; a build to host links at -O3

$ErrorActionPreference = "Stop"

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$recorderDir = Join-Path $rootDir "Binaries\Windows-Runtime\WebRecorder"
$webSdk = Join-Path $rootDir "Binaries\Web\Release\Sdk\CMake\HyperionWebSdk.cmake"
$templateDir = Join-Path $rootDir "Source\Templates\Platforms\Web"
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

function Get-NewestWriteTime([string[]]$paths)
{
    $newest = [DateTime]::MinValue

    foreach ($path in $paths)
    {
        if (Test-Path $path)
        {
            $item = Get-ChildItem $path -Recurse -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1

            if ($item -and $item.LastWriteTime -gt $newest)
            {
                $newest = $item.LastWriteTime
            }
        }
    }

    return $newest
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
if ($RecordSeconds -le 0) { $RecordSeconds = [int](Get-ProjectSetting "RecordSeconds" "60") }
if ($Worlds -eq "") { $Worlds = Get-ProjectSetting "Worlds" $editWorld }

$worldList = @($Worlds -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
$gameSourceDir = Join-Path $Project "Source"
$hasGameCode = Test-Path (Join-Path $gameSourceDir "Game")

$buildDir = Join-Path $Project "Build\Web"
$packageDir = Join-Path $buildDir "Package"
$shaderListsDir = Join-Path $buildDir "ShaderLists"
$recordedDir = Join-Path $buildDir "Recorded"
$recorderBuildDir = Join-Path $buildDir "Recorder"
$cmakeDir = Join-Path $buildDir "CMake"
$pageDir = Join-Path $buildDir "Page"
$outputDir = Join-Path $Project "Binaries\Web"
$listPath = Join-Path $recordedDir "engine-assets.txt"

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

if (-not (Get-Command python -ErrorAction SilentlyContinue))
{
    Fail "python not found on PATH"
}

if (-not (Test-Path $webSdk))
{
    Fail "The engine's web build is missing ($webSdk). Build it: Tools\Scripts\BuildHyperion.bat web"
}

# the cook reads the project, which only an editor build can
$cookDir = Join-Path $rootDir "Binaries\Windows\Release"
$cookExe = Join-Path $cookDir "BlobStorageCookCommandlet.exe"
$recorderSample = Join-Path $recorderDir "hyperion-sample.exe"

if (-not (Test-Path $cookExe))
{
    Fail "The editor build's cook commandlet is missing ($cookExe). Build the engine: Tools\Scripts\BuildHyperion.bat"
}

if (-not (Test-Path $recorderSample))
{
    Fail "The web recording runtime is missing ($recorderDir). Build it: Tools\Scripts\BuildHyperion.bat webrecorder"
}

function Invoke-Cook([string]$engineAssets, [string]$shaderCache)
{
    if (Test-Path $packageDir)
    {
        Remove-Item $packageDir -Recurse -Force
    }

    New-Item -ItemType Directory -Force $packageDir | Out-Null

    $cookArguments = @("--project=$Project", "--out-cache=$packageDir\Cache", "--out-content=$packageDir\Content", "--max-texture-size=$MaxTextureSize")

    if ($worldList.Count -gt 0)
    {
        $cookArguments += "--worlds=$($worldList -join ',')"
    }

    if ($engineAssets -ne "")
    {
        $cookArguments += "--engine-assets=$engineAssets"
    }

    if ($shaderCache -ne "")
    {
        $cookArguments += "--cachedir=$shaderCache"
    }
    else
    {
        # without one, the cook would take the shader lists the host tools' own cache holds
        $emptyShaderCache = Join-Path $buildDir "NoShaderLists"
        New-Item -ItemType Directory -Force $emptyShaderCache | Out-Null
        $cookArguments += "--cachedir=$emptyShaderCache"
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

    if ($engineAssets -ne "" -and -not (Select-String -Path $cookLog, $cookErrorLog -Pattern "listed asset\(s\)" -Quiet))
    {
        Fail "The cook ignored --engine-assets. Rebuild the recording runtime: Tools\Scripts\BuildHyperion.bat webrecorder"
    }

    $configDir = Join-Path $packageDir "Config"
    New-Item -ItemType Directory -Force $configDir | Out-Null
    Copy-Item (Join-Path $rootDir "Config\EngineConfig.Web.json") (Join-Path $configDir "EngineConfig.json")
    Copy-Item (Join-Path $rootDir "Config\GlobalConfig.Web.json") (Join-Path $configDir "GlobalConfig.json")
    Copy-Item (Join-Path $rootDir "Config\Shaders.hmf"), (Join-Path $rootDir "Config\DeviceTiers.hmf") $configDir
}

# 2. record what the level uses, when the recorded lists are older than what they were recorded from

$contentFolders = @("Worlds", "Scenes", "Materials", "Meshes", "Prefabs", "Textures", "InstanceData", "PhysicsShapes", "Source\Game") | ForEach-Object { Join-Path $Project $_ }
$recordedTime = if (Test-Path $listPath) { (Get-Item $listPath).LastWriteTime } else { [DateTime]::MinValue }
$sourcesTime = Get-NewestWriteTime (@((Join-Path $rootDir "Source\Shaders")) + $contentFolders)
$needsRecording = $Record -or ($recordedTime -lt $sourcesTime)

if ($NoRecord)
{
    $needsRecording = $false
}

$progress = if ($needsRecording) { @{ Narrowing = 44; Cooking = 46; Page = 55 } } else { @{ Narrowing = 2; Cooking = 4; Page = 20 } }

if ($needsRecording)
{
    if (-not (Test-Path (Join-Path $packageDir "Cache\toc.bin")))
    {
        Step 2 "Cooking everything once, to record the level from"
        Invoke-Cook "" ""
    }

    $runExe = $recorderSample

    if ($hasGameCode)
    {
        Step 10 "Building the game code against the recording runtime"

        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $vsDir = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

        if (-not $vsDir)
        {
            Fail "Visual Studio with the C++ tools not found"
        }

        $vcVars = Join-Path $vsDir "VC\Auxiliary\Build\vcvars64.bat"
        $cmakeExe = Join-Path $vsDir "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        $ninjaExe = (Join-Path $vsDir "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe") -replace '\\', '/'
        $recorderBinDir = Join-Path $recorderBuildDir "Bin"
        $sdkDir = (Join-Path $recorderDir "Sdk") -replace '\\', '/'

        Invoke-Cmd "call `"$vcVars`" >nul && `"$cmakeExe`" -S `"$gameSourceDir`" -B `"$recorderBuildDir\CMake`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninjaExe`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl.exe -DHYPERION_SDK_DIR=`"$sdkDir`" -DGAME_OUTPUT_DIR=`"$($recorderBinDir -replace '\\', '/')`" && `"$cmakeExe`" --build `"$recorderBuildDir\CMake`"" `
            "Building the game code against the recording runtime failed"

        $runExe = Join-Path $recorderBinDir "$name.exe"

        if (-not (Test-Path $runExe))
        {
            Fail "$runExe not found after building the game code"
        }
    }

    $runDir = Split-Path $runExe
    $runConfigDir = Join-Path $runDir "Config"
    New-Item -ItemType Directory -Force $runConfigDir | Out-Null

    # read in place of the runtime's own configs through --configvariant=Web
    $engineConfig = Get-Content (Join-Path $rootDir "Config\EngineConfig.Web.json") -Raw

    if ($FrameRate -gt 0)
    {
        $engineConfig = $engineConfig -replace '"TargetFrameRate":\s*\d+', "`"TargetFrameRate`": $FrameRate"
    }

    Set-Content (Join-Path $runConfigDir "EngineConfig.Web.json") $engineConfig -Encoding ascii
    Copy-Item (Join-Path $rootDir "Config\GlobalConfig.Web.json") (Join-Path $runConfigDir "GlobalConfig.Web.json") -Force

    $workDir = Join-Path $recordedDir "Work"

    if (Test-Path $workDir)
    {
        Remove-Item $workDir -Recurse -Force
    }

    New-Item -ItemType Directory -Force $workDir | Out-Null
    Copy-Item (Join-Path $packageDir "Cache") (Join-Path $workDir "Cache") -Recurse
    Copy-Item (Join-Path $packageDir "Content") (Join-Path $workDir "Content") -Recurse

    $workListPath = Join-Path $workDir "engine-assets.txt"

    function Invoke-RecordingRun([int]$percent, [string]$label, [string[]]$extraArguments)
    {
        $arguments = @("--configvariant=Web", "--basedir=$($rootDir -replace '\\', '/')", "--singleplayer=true", "--cooked=true",
            "--cachedir=$workDir\Cache", "--contentdir=$workDir\Content") + $extraArguments

        if ($worldList.Count -gt 0)
        {
            $arguments += "--startupworld=$($worldList[0])"
        }

        Step $percent "$label, $RecordSeconds seconds"

        $process = Start-Process -FilePath $runExe -ArgumentList (Format-Arguments $arguments) -WorkingDirectory $runDir -PassThru `
            -RedirectStandardOutput (Join-Path $workDir "$label.log") -RedirectStandardError (Join-Path $workDir "$label.err.log")

        if (-not $process.WaitForExit($RecordSeconds * 1000))
        {
            $process.CloseMainWindow() | Out-Null

            if (-not $process.WaitForExit(30000))
            {
                Stop-Process -Id $process.Id -Force
            }
        }
    }

    $recordStart = Get-Date

    # a shader variant compiled during a run is not in that run's list, so the first run only compiles
    Invoke-RecordingRun 20 "Recording run 1 of 2 (compiling)" @()
    Invoke-RecordingRun 32 "Recording run 2 of 2 (recording)" @("--record-engine-assets=$workListPath")

    $recordedFiles = @($workListPath, (Join-Path $workDir "Cache\shaderprops.bin"), (Join-Path $workDir "Cache\shaderpreload.bin"))

    foreach ($file in $recordedFiles)
    {
        if (-not (Test-Path $file) -or (Get-Item $file).LastWriteTime -lt $recordStart)
        {
            Fail "The recording run did not write $file. See the logs in $workDir"
        }
    }

    $recordedFiles += Join-Path $workDir "Cache\pipelinevariants.bin"

    foreach ($file in $recordedFiles)
    {
        if (Test-Path $file)
        {
            Copy-Item $file $recordedDir -Force
        }
    }

    Remove-Item (Join-Path $workDir "Cache"), (Join-Path $workDir "Content") -Recurse -Force

    Write-Host "Recorded $((Get-Content $listPath | Measure-Object -Line).Lines) engine assets"
}

# 3. cook what the level uses

$engineAssets = ""
$shaderCache = ""

if (Test-Path $listPath)
{
    Step $progress.Narrowing "Narrowing the recorded engine assets to the web's shader variants"

    $engineAssets = Join-Path $buildDir "EngineAssets.txt"

    $filter = Start-Process -FilePath python -ArgumentList @("`"$(Join-Path $PSScriptRoot 'FilterEngineAssetList.py')`"", "`"$listPath`"", "`"$(Join-Path $rootDir 'Content\Engine')`"", "WEBGPU", "WEB", "`"$engineAssets`"") `
        -Wait -NoNewWindow -PassThru

    if ($filter.ExitCode -ne 0)
    {
        Fail "Filtering the recorded engine assets failed"
    }

    if (Test-Path (Join-Path $recordedDir "shaderpreload.bin"))
    {
        if (Test-Path $shaderListsDir)
        {
            Remove-Item $shaderListsDir -Recurse -Force
        }

        New-Item -ItemType Directory -Force $shaderListsDir | Out-Null

        foreach ($fileName in @("shaderprops.bin", "shaderpreload.bin", "pipelinevariants.bin"))
        {
            $file = Join-Path $recordedDir $fileName

            if (Test-Path $file)
            {
                Copy-Item $file $shaderListsDir
            }
        }

        $shaderCache = $shaderListsDir
    }
}
else
{
    Write-Host "WARNING: nothing recorded, so all engine content is cooked"
}

Step $progress.Cooking "Cooking"
Invoke-Cook $engineAssets $shaderCache

# 4. the page: the launch layer and the project's game code, linked against the engine's web build

Step $progress.Page "Building the page"

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

# 5. stage everything a host needs into Binaries/Web, leaving anything else the folder holds (a CNAME, say) alone

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

# 6. serve it, when asked

if ($Run)
{
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

exit 0
