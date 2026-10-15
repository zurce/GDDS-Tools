$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (!(Test-Path (Join-Path $RepoRoot "src"))) {
    $RepoRoot = Split-Path -Parent $RepoRoot
}

$VcvarsBat = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
$DsInc = Join-Path $RepoRoot "Dependencies\DirectStorage\include"
$DsLib = Join-Path $RepoRoot "Dependencies\DirectStorage\lib\x64"
$Src = Join-Path $RepoRoot "src\main.cpp"
$OutExe = Join-Path $RepoRoot "bin\gdds_tool.exe"
$BinDir = Join-Path $RepoRoot "bin"

if (!(Test-Path $VcvarsBat)) {
    $Found = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
    if ($Found) { $VcvarsBat = $Found }
    else { throw "Could not locate vcvars64.bat" }
}

if (!(Test-Path $BinDir)) {
    New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
}

Write-Host "Building gdds_tool.exe (x64 Release)..." -ForegroundColor Cyan
$cmd = "`"$VcvarsBat`" && cl.exe /O2 /std:c++20 /EHsc /utf-8 /I`"$DsInc`" `"$Src`" /link /LIBPATH:`"$DsLib`" dstorage.lib /OUT:`"$OutExe`""
cmd.exe /c $cmd

if ($LASTEXITCODE -eq 0) {
    Copy-Item (Join-Path $DsLib "dstorage.dll") $BinDir -Force
    Copy-Item (Join-Path $DsLib "dstoragecore.dll") $BinDir -Force
    Remove-Item (Join-Path $RepoRoot "main.obj") -ErrorAction SilentlyContinue
    Write-Host "Build Successful: $OutExe" -ForegroundColor Green
} else {
    Write-Host "Build Failed with exit code $LASTEXITCODE" -ForegroundColor Red
}
