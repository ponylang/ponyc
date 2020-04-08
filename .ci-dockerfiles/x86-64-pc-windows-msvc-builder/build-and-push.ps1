# You should already be logged in to DockerHub when you run this.
$ErrorActionPreference = 'Stop'

$today = (Get-Date -Format 'yyyyMMdd')
$dockerfileDir = Split-Path $script:MyInvocation.MyCommand.Path

$dockerTag = "ponylang/ponyc-ci-x86-64-pc-windows-msvc-builder:$today"
docker build --pull -t "$dockerTag" "$dockerfileDir"
docker push "$dockerTag"
