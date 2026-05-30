[CmdletBinding()]
param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$Target = "RaidParty",
    [int]$PollMs = 350,
    [int]$DebounceMs = 500,
    [switch]$RunOnce,
    [switch]$NoLaunch
)

$ErrorActionPreference = "Continue"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildPath = Join-Path $Root $BuildDir
$LivePath = Join-Path $BuildPath "live"
$script:GameProcess = $null

function Get-WatchStamp {
    $files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    $srcPath = Join-Path $Root "src"
    $dataPath = Join-Path $Root "assets"
    $cmakePath = Join-Path $Root "CMakeLists.txt"

    if (Test-Path $srcPath) {
        Get-ChildItem -Path $srcPath -Recurse -File -Include *.c,*.h,*.cpp,*.hpp |
            ForEach-Object { [void]$files.Add($_) }
    }

    if (Test-Path $dataPath) {
        Get-ChildItem -Path $dataPath -Recurse -File -Include *.json |
            ForEach-Object { [void]$files.Add($_) }
    }

    if (Test-Path $cmakePath) {
        [void]$files.Add((Get-Item $cmakePath))
    }

    if ($files.Count -eq 0) {
        return ""
    }

    return (($files |
        Sort-Object FullName |
        ForEach-Object { "{0}|{1}|{2}" -f $_.FullName, $_.Length, $_.LastWriteTimeUtc.Ticks }) -join "`n")
}

function Get-BuildExeCandidates {
    return @(
        (Join-Path (Join-Path $BuildPath $Config) "$Target.exe"),
        (Join-Path $BuildPath "$Target.exe")
    )
}

function Get-BuiltExePath {
    foreach ($path in (Get-BuildExeCandidates)) {
        if (Test-Path $path) {
            return (Resolve-Path $path).Path
        }
    }
    return $null
}

function Stop-ProcessGracefully {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) { return }

    try {
        $Process.Refresh()
        if ($Process.HasExited) { return }

        if ($Process.MainWindowHandle -ne 0) {
            [void]$Process.CloseMainWindow()
            if ($Process.WaitForExit(1500)) { return }
        }

        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    } catch {
        # Process may have already exited
    }
}

function Stop-ProcessesUsingPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) { return }

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $processName = [System.IO.Path]::GetFileNameWithoutExtension($fullPath)

    Get-Process -Name $processName -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            $processPath = $_.Path
        } catch {
            $processPath = $null
        }

        if ($processPath) {
            $candidatePath = [System.IO.Path]::GetFullPath($processPath)
            if ([string]::Equals($candidatePath, $fullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                Stop-ProcessGracefully $_
            }
        }
    }
}

function Invoke-CMakeBuild {
    if (-not (Test-Path (Join-Path $BuildPath "CMakeCache.txt"))) {
        Write-Host "Configuring CMake..."
        & cmake -S $Root -B $BuildPath
        if ($LASTEXITCODE -ne 0) { return $false }
    }

    # Delete old exe to force MSBuild to always relink
    foreach ($candidate in (Get-BuildExeCandidates)) {
        if (Test-Path $candidate) { Remove-Item -Path $candidate -Force }
    }

    Write-Host "Building $Target ($Config)..."
    & cmake --build $BuildPath --config $Config --target $Target
    return ($LASTEXITCODE -eq 0)
}

function Copy-BuildToLivePath {
    param([string]$SourceExe)

    $sourceDir = Split-Path -Parent $SourceExe
    $liveExe = Join-Path $LivePath (Split-Path -Leaf $SourceExe)

    Stop-ProcessGracefully $script:GameProcess
    $script:GameProcess = $null
    Stop-ProcessesUsingPath $liveExe

    New-Item -ItemType Directory -Force -Path $LivePath | Out-Null
    Copy-Item -Path (Join-Path $sourceDir "*") -Destination $LivePath -Recurse -Force

    return $liveExe
}

function Invoke-RebuildAndLaunch {
    param([string]$Reason)

    Write-Host ""
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $Reason"

    foreach ($candidate in (Get-BuildExeCandidates)) {
        Stop-ProcessesUsingPath $candidate
    }

    if (-not (Invoke-CMakeBuild)) {
        Write-Host "Build failed. Check the errors above, fix the file, save again." -ForegroundColor Red
        Write-Host "The watcher will retry on the next save." -ForegroundColor Cyan
        return
    }

    if ($NoLaunch) {
        Write-Host "Build succeeded." -ForegroundColor Green
        return
    }

    $sourceExe = Get-BuiltExePath
    if (-not $sourceExe) {
        Write-Host "Build succeeded but executable not found - skipping launch." -ForegroundColor Yellow
        return
    }

    $liveExe = Copy-BuildToLivePath $sourceExe

    Write-Host "Launching $liveExe" -ForegroundColor Green
    $script:GameProcess = Start-Process -FilePath $liveExe -WorkingDirectory $Root -PassThru
}

# -- Main ----------------------------------------------------------

Set-Location $Root

$lastStamp = Get-WatchStamp
Invoke-RebuildAndLaunch "Initial build"
$lastStamp = Get-WatchStamp

if ($RunOnce) { return }

Write-Host ""
Write-Host "Watching src, assets, and CMakeLists.txt. Save to rebuild and relaunch."
Write-Host "Press Ctrl+C to stop."

while ($true) {
    Start-Sleep -Milliseconds $PollMs
    $currentStamp = Get-WatchStamp

    if ($currentStamp -ne $lastStamp) {
        Start-Sleep -Milliseconds $DebounceMs
        $currentStamp = Get-WatchStamp

        if ($currentStamp -ne $lastStamp) {
            $lastStamp = $currentStamp
            Invoke-RebuildAndLaunch "Change detected"
            $lastStamp = Get-WatchStamp
        }
    }
}
