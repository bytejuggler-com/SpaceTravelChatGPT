param([switch]$Run, [switch]$Package)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $projectRoot
function Fetch-Archive($Url, $Archive, $Hash) {
    if (-not (Test-Path $Archive)) { Invoke-WebRequest $Url -OutFile $Archive }
    if ((Get-FileHash $Archive -Algorithm SHA256).Hash -ne $Hash) { throw "Dependency checksum mismatch: $Archive" }
}
New-Item -ItemType Directory -Force external | Out-Null
if (-not (Test-Path external/SDL3-3.4.18/include/SDL3/SDL.h)) {
    Fetch-Archive 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-VC.zip' 'external/sdl.zip' '78D84602AE616CFE26B33A73B7D3B9B6B56E8A6650E53D92C9C6CA938F747ACD'
    Expand-Archive external/sdl.zip external -Force
}
if (-not (Test-Path external/imgui/imgui.cpp)) {
    Fetch-Archive 'https://api.github.com/repos/ocornut/imgui/zipball/v1.93.0' 'external/imgui.zip' '0F1E2B31DBF754154576C7C668B638B74D524FB77DF34A2526CEF035A90231EB'
    Expand-Archive external/imgui.zip external/imgui-extract -Force
    $imguiSource = Get-ChildItem external/imgui-extract -Directory | Select-Object -First 1
    Copy-Item -LiteralPath $imguiSource.FullName -Destination external/imgui -Recurse
}
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw 'Install Visual Studio 2022 with Desktop development with C++.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
& $cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& $cmake --build build --config Release --parallel 1
if ($LASTEXITCODE) { throw 'Build failed.' }
& (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed.' }
if ($Package) {
    New-Item -ItemType Directory -Force dist/Astra/licenses | Out-Null
    Copy-Item build/Release/Astra.exe,build/Release/SDL3.dll dist/Astra -Force
    Copy-Item build/Release/shaders,build/Release/config dist/Astra -Recurse -Force
    Copy-Item README.md dist/Astra -Force
    Copy-Item external/SDL3-3.4.18/LICENSE.txt dist/Astra/licenses/SDL3.txt -Force
    Copy-Item external/imgui/LICENSE.txt dist/Astra/licenses/Dear-ImGui.txt -Force
    # Include only release files, never local logs, screenshots or saved preferences.
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $packageRoot = Join-Path $projectRoot 'dist/Astra'
    $zipPath = Join-Path $projectRoot 'dist/Astra-Windows-x64.zip'
    $zipFile = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::Create)
    $zip = [System.IO.Compression.ZipArchive]::new($zipFile, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $files = @('Astra.exe','SDL3.dll','README.md','config/demo.cfg','licenses/SDL3.txt','licenses/Dear-ImGui.txt')
        $files += Get-ChildItem (Join-Path $packageRoot 'shaders') -Filter '*.dxil' | ForEach-Object { 'shaders/' + $_.Name }
        foreach ($relative in $files) {
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $packageRoot $relative),('Astra/' + $relative)) | Out-Null
        }
    } finally { $zip.Dispose(); $zipFile.Dispose() }
}
if ($Run) { Start-Process -FilePath (Join-Path $projectRoot 'build/Release/Astra.exe') -WorkingDirectory $projectRoot }
