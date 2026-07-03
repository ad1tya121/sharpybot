$ErrorActionPreference = "Stop"

$RepoUrl = "https://github.com/ad1tya121/sharpybot.git"
$RepoDir = "sharpybot"

if (Test-Path "$RepoDir/.git") {
    Write-Host "==> Updating existing checkout..."
    git -C $RepoDir pull --ff-only
} else {
    Write-Host "==> Cloning sharpybot..."
    git clone $RepoUrl $RepoDir
}

Set-Location "$RepoDir/engine"

$BuildDir = "build"
Write-Host "==> Configuring..."

$CmakeArgs = @("-S", ".", "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=Release")

if (Get-Command g++ -ErrorAction SilentlyContinue) {
    Write-Host "--> Detected GCC/MinGW, using MinGW Makefiles generator..."
    $CmakeArgs += @("-G", "MinGW Makefiles")
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    Write-Host "--> Detected Visual Studio compiler..."
} else {
    Write-Host "--> No direct CLI compiler found. Defaulting to Visual Studio generator..."
    $CmakeArgs += @("-G", "Visual Studio 17 2022")
}

cmake @CmakeArgs

Write-Host "==> Building..."
cmake --build $BuildDir --config Release

$Bin = Get-ChildItem -Path $BuildDir -Recurse -Filter "engine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $Bin) {
    throw "Could not find built engine.exe under $BuildDir"
}

Copy-Item -Force model.bin $Bin.DirectoryName

Write-Host "==> Build complete."
Write-Host "    Engine binary: $($Bin.FullName)"
Write-Host "    (point Cute Chess to this file, with working directory set to the same folder)"
