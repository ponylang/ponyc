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

### this doesn't account for master changing commit, assumes we are HEAD
# or can otherwise push without issue. that shouldl error out without issue.
# leaving us to restart from a different HEAD commit
git checkout master
git pull

# update VERSION
echo -e "\e[34mUpdating VERSION to ${VERSION}\e[0m"
echo "${VERSION}" > VERSION

# version the changelog
echo -e "\e[34mUpdating CHANGELOG.md for release\e[0m"
changelog-tool release "${VERSION}" -e

# commit CHANGELOG and VERSION updates
echo -e "\e[34mCommiting VERSION and CHANGELOG.md changes\e[0m"
git add CHANGELOG.md VERSION
git commit -m "${VERSION} release"

# tag release
echo -e "\e[34mTagging for release\e[0m"
git tag "${VERSION}"

# push release to remote
echo -e "\e[34mPushing commited changes back to master\e[0m"
git push ${PUSH_TO} master
echo -e "\e[34mPushing ${VERSION} tag. Release artifact building will start on success.\e[0m"
git push ${PUSH_TO} "${VERSION}"

# release body
echo -e "\e[34mPreparing to update GitHub release notes...\e[0m"

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

echo -e "\e[34mUploading release notes...\e[0m"

result=$(curl -X POST "https://api.github.com/repos/${GITHUB_REPOSITORY}/releases" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -u "${GITHUB_ACTOR}:${RELEASE_TOKEN}" \
  --data "${json}")

rslt_scan=$(echo "${result}" | jq -r '.id')
if [ "$rslt_scan" != null ]
then
  echo -e "\e[34mRelease notes uploaded\e[0m"
else
  echo "\e[31mUnable to upload release notes, here's the curl output...\e[0m"
  echo "${result}"
  exit 1
fi

# pull again, just in case, odds of this being needed are really slim
git pull

# update CHANGELOG for new entries
echo -e "\e[34mAdding new 'unreleased' section to CHANGELOG.md\e[0m"
changelog-tool unreleased -e

# commit changelog and push to master
echo -e "\e[34mCommiting CHANGELOG.md change\e[0m"
git add CHANGELOG.md
git commit -m "Add unreleased section to CHANGELOG post ${VERSION} release [skip ci]"

# delete release-VERSION tag
echo -e "\e[34mDeleting no longer needed remote tag release-${VERSION}\e[0m"
git push --delete ${PUSH_TO} "release-${VERSION}"
