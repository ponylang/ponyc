$version = "3.9.1"

$arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
if ($arch -ieq "X64") { $zipArch = "x64" }
elseif ($arch -ieq "Arm64") { $zipArch = "ARM64" }
else { throw "Unsupported architecture: $arch" }

$zipName = "libressl_v${version}_windows_${zipArch}.zip"
$url = "https://github.com/libressl/portable/releases/download/v${version}/${zipName}"

$installDir = "C:\libressl"
$tempZip = Join-Path $env:TEMP $zipName

Invoke-WebRequest $url -OutFile $tempZip
Expand-Archive -Force -Path $tempZip -DestinationPath $installDir

Add-Content $env:GITHUB_ENV "LIB=$installDir\lib;$env:LIB"
Add-Content $env:GITHUB_ENV "OPENSSL_ROOT_DIR=$installDir"
