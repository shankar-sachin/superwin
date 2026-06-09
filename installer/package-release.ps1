# Builds the two release artifacts into installer\Output, both named from the
# version in src\Version.h so they always match the appcast:
#
#   * SuperWin_v<version>.exe          -- the Inno Setup wizard (fresh installs)
#   * SuperWin_v<version>_portable.zip -- the files the in-app updater downloads
#                                         and extracts over the install folder
#
# The portable zip is built from build\Release with the SAME exclusions the
# installer's [Files] block uses, so an auto-updated app has exactly the files a
# freshly-installed one does. (A hand-zipped, partial folder is the usual reason
# "the auto-updater is broken even though I'm uploading the portable zips.")
#
# Usage (from a Developer prompt, after a Release build of the SuperWin target):
#   pwsh installer\package-release.ps1            # zip + build installer
#   pwsh installer\package-release.ps1 -ZipOnly   # just the portable zip
[CmdletBinding()]
param(
    [switch]$ZipOnly  # skip running ISCC; only (re)build the portable zip
)

$ErrorActionPreference = 'Stop'
$repo    = Split-Path -Parent $PSScriptRoot
$release = Join-Path $repo 'build\Release'
$outDir  = Join-Path $PSScriptRoot 'Output'
$exe     = Join-Path $release 'SuperWin.exe'

if (-not (Test-Path $exe)) {
    throw "build\Release\SuperWin.exe not found. Build the Release SuperWin target first:`n" +
          "  cmake --preset msvc -DSUPERWIN_ENABLE_WINUI=ON; cmake --build --preset msvc-release --target SuperWin"
}

# Single source of truth for the version.
$verLine = Select-String -Path (Join-Path $repo 'src\Version.h') -Pattern 'SUPERWIN_VERSION_STRING\s+"([^"]+)"'
if (-not $verLine) { throw 'Could not read SUPERWIN_VERSION_STRING from src\Version.h' }
$version = $verLine.Matches[0].Groups[1].Value
Write-Host "Packaging SuperWin v$version" -ForegroundColor Cyan

New-Item -ItemType Directory -Force $outDir | Out-Null

# ---- portable zip (matches installer\SuperWin.iss [Files] exclusions) ----
$excludeExact = @('SuperWin_tests.exe', 'app_only.pri')
$excludeExt   = @('.pdb', '.ilk', '.exp', '.lib')
$excludeLike  = @('pri_dump*', 'Catch2*', '*.WebView2')

$staging = Join-Path ([System.IO.Path]::GetTempPath()) ("superwin_pkg_" + [guid]::NewGuid())
New-Item -ItemType Directory -Force $staging | Out-Null
try {
    Get-ChildItem -Path $release -Recurse -File | ForEach-Object {
        $name = $_.Name
        if ($excludeExact -contains $name) { return }
        if ($excludeExt -contains $_.Extension.ToLower()) { return }
        foreach ($p in $excludeLike) { if ($name -like $p) { return } }
        # Files INSIDE an excluded directory (e.g. the SuperWin.exe.WebView2 cache
        # left behind by running the app from build\Release) -- match by path, the
        # way the .iss "\*.WebView2\*" exclude does, not just by leaf name.
        if ($_.FullName -like '*.WebView2\*') { return }
        $rel = $_.FullName.Substring($release.Length).TrimStart('\')
        $dest = Join-Path $staging $rel
        New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
        Copy-Item $_.FullName $dest
    }
    if (-not (Test-Path (Join-Path $staging 'SuperWin.exe'))) { throw 'staging is missing SuperWin.exe' }

    $zip = Join-Path $outDir "SuperWin_v${version}_portable.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    # Zip the staging *contents* (no wrapper folder) so the updater extracts files
    # straight into the install dir.
    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zip -CompressionLevel Optimal
    $mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
    Write-Host "  portable zip -> $zip ($mb MB)" -ForegroundColor Green
} finally {
    Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue
}

# ---- installer wizard ----
if (-not $ZipOnly) {
    $iscc = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
    if (-not (Test-Path $iscc)) { throw "Inno Setup 6 not found at $iscc (install it, or pass -ZipOnly)." }
    & $iscc (Join-Path $PSScriptRoot 'SuperWin.iss')
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }
    Write-Host "  installer    -> $outDir\SuperWin_v${version}.exe" -ForegroundColor Green
}

Write-Host "`nUpload BOTH assets to the GitHub release tagged v$version, then commit & push" -ForegroundColor Cyan
Write-Host "installer\appcast.xml (with the new <item>) so the updater can see it." -ForegroundColor Cyan
