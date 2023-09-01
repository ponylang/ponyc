# You should already be logged in to DockerHub and GitHub Container Registry
# when you run this.
$ErrorActionPreference = 'Stop'

$dockerfileDir = Split-Path $script:MyInvocation.MyCommand.Path

## DockerHub

docker build --pull -t "ponylang/ponyc:windows" $dockerfileDir
docker push "ponylang/ponyc:windows"

## GitHub Container Registry

docker build --pull -t "ghcr.io/ponylang/ponyc:windows" $dockerfileDir
docker push "ghcr.io/ponylang/ponyc:windows"
