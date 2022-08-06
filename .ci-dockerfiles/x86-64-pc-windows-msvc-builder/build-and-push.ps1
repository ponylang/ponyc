# You should already be logged in to DockerHub when you run this.
$ErrorActionPreference = 'Stop'

$name = "ponylang/ponyc-ci-x86-64-pc-windows-msvc-builder"
$today = (Get-Date -Format 'yyyyMMdd')
$dockerfileDir = Split-Path $script:MyInvocation.MyCommand.Path

docker build --pull -t "$name:$today" "$dockerfileDir"
docker push "$name:$today"
