# Download and build the sharpybot chess engine on Windows.
# Usage: powershell -ExecutionPolicy Bypass -File .\run-sharpybot.ps1
$ErrorActionPreference = "Stop"

$RepoUrl = "https://github.com/ad1tya121/sharpybot.git"
$RepoDir = "sharpybot"

# --- 1. Get the code ---------------------------------------------------------
if (Test-Path "$RepoDir/.git") {
    Write-Host "==> Updating existing checkout..."
    git -C $RepoDir pull --ff-only
} else {
    Write-Host "==> Cloning sharpybot..."
    git clone $RepoUrl $RepoDir
}

Set-Location "$RepoDir/engine"

# --- 2. Configure & build -----------------------------------------------------
$BuildDir = "build"
Write-Host "==> Configuring..."
cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=Release

Write-Host "==> Building..."
cmake --build $BuildDir --config Release

# --- 3. Locate the built binary ------------------------------------------------
$Bin = Get-ChildItem -Path $BuildDir -Recurse -Filter "engine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $Bin) {
    throw "Could not find built engine.exe under $BuildDir"
}

# --- 4. model.bin must sit next to the executable (loaded via relative path) --
Copy-Item -Force model.bin $Bin.DirectoryName

Write-Host "==> Build complete."
Write-Host "    Engine binary: $($Bin.FullName)"
Write-Host "    (point Cute Chess to this file, with working directory set to the same folder)"
