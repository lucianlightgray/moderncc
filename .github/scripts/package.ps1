# Package the installed mcc tree (in .\stage) into per-artifact zips (Windows).
#   -Ref  git ref name (e.g. v1.2.3)   -Plat platform slug (e.g. windows-x86_64)
# Produces, under .\out:
#   mcc-<ver>-<plat>.zip      static (/MT) + stripped mcc.exe + runtime
#   libmcc-<ver>-<plat>.zip   libmcc.lib/.a + headers + cmake package + runtime
#   checksums-<plat>.txt      sha256 of the zips
# No cross bundle on Windows (the cross compilers are host-fragile there).
param([Parameter(Mandatory)][string]$Ref, [Parameter(Mandatory)][string]$Plat)
$ErrorActionPreference = 'Stop'

$ver   = $Ref -replace '^v',''
$stage = Join-Path $PWD 'stage'
$pkg   = Join-Path $PWD 'pkg'
$out   = Join-Path $PWD 'out'
Remove-Item -Recurse -Force $pkg,$out -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $pkg,$out | Out-Null

# 1) mcc: the compiler executable + its runtime.
$d = "mcc-$ver-$Plat"
New-Item -ItemType Directory -Force -Path "$pkg\$d\bin","$pkg\$d\lib" | Out-Null
Copy-Item "$stage\bin\mcc.exe" "$pkg\$d\bin\"
if (Test-Path "$stage\lib\mcc") { Copy-Item -Recurse "$stage\lib\mcc" "$pkg\$d\lib\" }
Compress-Archive -Path "$pkg\$d" -DestinationPath "$out\$d.zip" -Force

# 2) libmcc: the embeddable static library, header, cmake package, runtime.
$d = "libmcc-$ver-$Plat"
New-Item -ItemType Directory -Force -Path "$pkg\$d\lib" | Out-Null
Copy-Item -Recurse "$stage\include" "$pkg\$d\"
Get-ChildItem "$stage\lib" -Filter 'libmcc.*' -File | Copy-Item -Destination "$pkg\$d\lib\"
if (Test-Path "$stage\lib\cmake") { Copy-Item -Recurse "$stage\lib\cmake" "$pkg\$d\lib\" }
if (Test-Path "$stage\lib\mcc")   { Copy-Item -Recurse "$stage\lib\mcc"   "$pkg\$d\lib\" }
Compress-Archive -Path "$pkg\$d" -DestinationPath "$out\$d.zip" -Force

# checksums
Push-Location $out
Get-FileHash -Algorithm SHA256 *.zip |
  ForEach-Object { "{0}  {1}" -f $_.Hash.ToLower(), (Split-Path $_.Path -Leaf) } |
  Set-Content "checksums-$Plat.txt"
Pop-Location
Write-Host "== packaged =="
Get-ChildItem $out
