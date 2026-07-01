






param([Parameter(Mandatory)][string]$Ref, [Parameter(Mandatory)][string]$Plat)
$ErrorActionPreference = 'Stop'

$ver   = $Ref -replace '^v',''
$stage = Join-Path $PWD 'stage'
$pkg   = Join-Path $PWD 'pkg'
$out   = Join-Path $PWD 'out'
Remove-Item -Recurse -Force $pkg,$out -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $pkg,$out | Out-Null


$d = "mcc-$ver-$Plat"
New-Item -ItemType Directory -Force -Path "$pkg\$d\bin","$pkg\$d\lib" | Out-Null
Copy-Item "$stage\bin\mcc.exe" "$pkg\$d\bin\"
if (Test-Path "$stage\lib\mcc") { Copy-Item -Recurse "$stage\lib\mcc" "$pkg\$d\lib\" }
Compress-Archive -Path "$pkg\$d" -DestinationPath "$out\$d.zip" -Force


$d = "libmcc-$ver-$Plat"
New-Item -ItemType Directory -Force -Path "$pkg\$d\lib" | Out-Null
Copy-Item -Recurse "$stage\include" "$pkg\$d\"
Get-ChildItem "$stage\lib" -Filter 'libmcc.*' -File | Copy-Item -Destination "$pkg\$d\lib\"
if (Test-Path "$stage\lib\cmake") { Copy-Item -Recurse "$stage\lib\cmake" "$pkg\$d\lib\" }
if (Test-Path "$stage\lib\mcc")   { Copy-Item -Recurse "$stage\lib\mcc"   "$pkg\$d\lib\" }
Compress-Archive -Path "$pkg\$d" -DestinationPath "$out\$d.zip" -Force


Push-Location $out
Get-FileHash -Algorithm SHA256 *.zip |
  ForEach-Object { "{0}  {1}" -f $_.Hash.ToLower(), (Split-Path $_.Path -Leaf) } |
  Set-Content "checksums-$Plat.txt"
Pop-Location
Write-Host "== packaged =="
Get-ChildItem $out
