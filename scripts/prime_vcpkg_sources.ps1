# BrassCue - GPL-3.0. Use official codeload for archives whose GitHub redirect returns 504.
# Port SHA512 values remain authoritative: never change or bypass verification.
param([Parameter(Mandatory=$true)][string]$VcpkgRoot)
$ErrorActionPreference = 'Stop'
$ports = @('egl-registry','opengl-registry','vcpkg-cmake','vcpkg-cmake-config','nanosvg','libpng','libjpeg-turbo','tiff','pcre2','zlib','expat','wxwidgets')
New-Item -ItemType Directory -Force "$VcpkgRoot/downloads" | Out-Null
foreach ($port in $ports) {
    $portfile = "$VcpkgRoot/ports/$port/portfile.cmake"
    if (!(Test-Path $portfile)) { continue }
    $content = Get-Content $portfile -Raw
    if ($content -notmatch 'vcpkg_from_github\s*\(' -or $content -match 'FILE_DISAMBIGUATOR') { continue }
    $content = [regex]::Match($content, '(?s)vcpkg_from_github\s*\(.*?\n\)').Value
    $repoMatch = [regex]::Match($content, '\bREPO\s+"?([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)')
    $refMatch = [regex]::Match($content, '\bREF\s+("[^"]+"|[^\s)]+)')
    $hashMatch = [regex]::Match($content, '\bSHA512\s+([A-Fa-f0-9]{128})')
    if (!$repoMatch.Success -or !$refMatch.Success -or !$hashMatch.Success) { continue }
    $repo = $repoMatch.Groups[1].Value
    $ref = $refMatch.Groups[1].Value.Trim('"')
    $metadata = Get-Content "$VcpkgRoot/ports/$port/vcpkg.json" -Raw | ConvertFrom-Json
    $version = @($metadata.version, $metadata.'version-semver', $metadata.'version-string', $metadata.'version-date') | Where-Object { $_ } | Select-Object -First 1
    $ref = $ref.Replace('${VERSION}', [string]$version)
    if ($ref -notmatch '^[A-Za-z0-9._-]+$') { continue }
    $expected = $hashMatch.Groups[1].Value.ToLowerInvariant()
    $destination = Join-Path "$VcpkgRoot/downloads" ($repo.Replace('/','-') + '-' + $ref + '.tar.gz')
    if ((Test-Path $destination) -and (Get-FileHash $destination -Algorithm SHA512).Hash.ToLowerInvariant() -eq $expected) { continue }
    $temporary = "$destination.brasscue-partial"
    try {
        & curl.exe --fail --location --retry 2 --retry-delay 1 "https://codeload.github.com/$repo/tar.gz/$ref" --output $temporary
        if ($LASTEXITCODE -ne 0) { throw "Official codeload download failed for $port" }
        if ((Get-FileHash $temporary -Algorithm SHA512).Hash.ToLowerInvariant() -ne $expected) { throw "Port SHA512 mismatch for $port" }
        Move-Item -Force $temporary $destination
        Write-Output "Verified official source cache for $port"
    } finally {
        if (Test-Path $temporary) { Remove-Item $temporary }
    }
}
