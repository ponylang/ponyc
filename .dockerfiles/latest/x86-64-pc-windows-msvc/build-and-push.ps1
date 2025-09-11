# You should already be logged in to GitHub Container Registry when you run
# this.
$ErrorActionPreference = 'Stop'

$dockerfileDir = Split-Path $script:MyInvocation.MyCommand.Path

## GitHub Container Registry

docker build --pull -t "ghcr.io/ponylang/ponyc:windows" $dockerfileDir
docker push "ghcr.io/ponylang/ponyc:windows"
