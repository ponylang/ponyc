#!/bin/bash

set -o errexit

# Verify ENV is set up correctly
# We validate all that need to be set in case, in an absolute emergency,
# we need to run this by hand. Otherwise the GitHub actions environment should
# provide all of these if properly configured
if [[ -z "${RELEASE_TOKEN}" ]]; then
  echo -e "\e[31mA personal access token needs to be set in RELEASE_TOKEN."
  echo -e "\e[31mIt should not be secrets.GITHUB_TOKEN. It has to be a"
  echo -e "\e[31mpersonal access token otherwise next steps in the release"
  echo -e "\e[31mprocess WILL NOT trigger."
  echo -e "\e[31mPersonal access tokens are in the form:"
  echo -e "\e[31m     TOKEN"
  echo -e "\e[31mfor example:"
  echo -e "\e[31m     1234567890"
  echo -e "\e[31mExiting.\e[0m"
  exit 1
fi

if [[ -z "${GITHUB_REF}" ]]; then
  echo -e "\e[31mThe release tag needs to be set in GITHUB_REF."
  echo -e "\e[31mThe tag should be in the following GitHub specific form:"
  echo -e "\e[31m    /refs/tags/release-X.Y.Z"
  echo -e "\e[31mwhere X.Y.Z is the version we are releasing"
  echo -e "\e[31mExiting.\e[0m"
  exit 1
fi

if [[ -z "${GITHUB_REPOSITORY}" ]]; then
  echo -e "\e[31mName of this repository needs to be set in GITHUB_REPOSITORY."
  echo -e "\e[31mShould be in the form OWNER/REPO, for example:"
  echo -e "\e[31m     ponylang/ponyup"
  echo -e "\e[31mExiting.\e[0m"
  exit 1
fi

# no unset variables allowed from here on out
# allow above so we can display nice error messages for expected unset variables
set -o nounset

# Set up GitHub credentials
git config --global user.name 'Ponylang Main Bot'
git config --global user.email 'ponylang.main@gmail.com'
git config --global push.default simple

PUSH_TO="https://${RELEASE_TOKEN}@github.com/${GITHUB_REPOSITORY}.git"

# Extract version from tag reference
# Tag ref version: "refs/tags/release-1.0.0"
# Version: "1.0.0"
VERSION="${GITHUB_REF/refs\/tags\/release-/}"

# create release prep branch
git checkout -b "prep-release-${VERSION}"

# update VERSION
echo "${VERSION}" > VERSION
echo "VERSION set to ${VERSION}"

# version the changelog
changelog-tool release "${VERSION}" -e

# commit CHANGELOG and VERSION updates
git add CHANGELOG.md VERSION
git commit -m "${VERSION} release"

# merge into release
git checkout release
if ! git diff --exit-code release origin/release
then
  echo "ERROR! There are local-only changes on branch 'release'!"
  exit 1
fi
echo "Merging changes into release"
git merge "prep-release-${VERSION}" -m "Release ${VERSION}"

# tag release
echo "Tagging version"
git tag "${VERSION}"

# push to release branch
echo "Pushing to release branch"
git push ${PUSH_TO} release
git push ${PUSH_TO} "${VERSION}"

# release body
echo "Preparing to update GitHub release notes..."

body=$(changelog-tool get "$VERSION")

jsontemplate="
{
  \"tag_name\":\$version,
  \"name\":\$version,
  \"body\":\$body
}
"

json=$(jq -n \
--arg version "$VERSION" \
--arg body "$body" \
"${jsontemplate}")

echo "Uploading release notes..."

result=$(curl -X POST "https://api.github.com/repos/${GITHUB_REPOSITORY}/releases" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -u "${GITHUB_ACTOR}:${GITHUB_TOKEN}" \
  --data "${json}")

rslt_scan=$(echo "${result}" | jq -r '.id')
if [ "$rslt_scan" != null ]
then
  echo "Release notes uploaded"
else
  echo "Unable to upload release notes, here's the curl output..."
  echo "${result}"
  exit 1
fi

# update CHANGELOG for new entries
git checkout master
git merge "prep-release-${VERSION}"
changelog-tool unreleased -e

# commit changelog and push to master
git add CHANGELOG.md
git commit -m "Add unreleased section to CHANGELOG post ${VERSION} release prep

[skip ci]"
git push ${PUSH_TO} master

# delete release-VERSION tag
git push --delete ${PUSH_TO} "release-${VERSION}"
