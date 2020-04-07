# You should already be logged in to DockerHub when you run this.
$ErrorActionPreference = 'Stop'

$dockerfileDir = Split-Path $script:MyInvocation.MyCommand.Path

docker build --pull -t "ponylang/ponyc:windows" $dockerfileDir
docker push "ponylang/ponyc:windows"
