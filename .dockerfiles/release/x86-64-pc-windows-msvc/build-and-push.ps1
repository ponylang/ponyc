# You should already be logged in to DockerHub when you run this.
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhitespace($env:VERSION))
{
    throw "The version number needs to be set in the VERSION environment variable (e.g. X.Y.Z)."
}

if ([string]::IsNullOrWhitespace($env:GITHUB_REPOSITORY))
{
    throw "The name of this repository needs to be set in the GITHUB_REPOSITORY environment variable (e.g. ponylang/ponyup)."
}

$dockerfileDir = Split-Path $script:MyInvocation.MyCommand.Path

$dockerTag = $env:GITHUB_REPOSITORY + ':' + $env:VERSION + '-windows'
docker build --pull -t "$dockerTag" "$dockerfileDir"
docker push "$dockerTag"

$dockerTag = $env:GITHUB_REPOSITORY + ':release-windows'
docker build --pull -t "$dockerTag" "$dockerfileDir"
docker push "$dockerTag"
