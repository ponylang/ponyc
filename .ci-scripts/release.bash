#!/bin/bash

set -o errexit
set -o nounset

# Set up .netrc file with GitHub credentials
git_setup ( ) {
  cat <<- EOF > $HOME/.netrc
        machine github.com
        login $GITHUB_ACTOR
        password $GITHUB_TOKEN
        machine api.github.com
        login $GITHUB_ACTOR
        password $GITHUB_TOKEN
EOF

  chmod 600 $HOME/.netrc

  git config --global user.name 'Ponylang Main Bot'
  git config --global user.email 'ponylang.main@gmail.com'
}

# Gather expected arguments.
if [ $# -lt 1 ]
then
  echo "Tag is required"
  exit 1
fi

TAG=$1
# changes tag from "release-1.0.0" to "1.0.0"
VERSION="${TAG/refs\/tags\/release-/}"

git_setup

# create release prep branch
git checkout -b "prep-release-${VERSION}"

# update VERSION
echo "${VERSION}" > VERSION
echo "VERSION set to ${VERSION}"

# version the changelog
changelog-tool release "${VERSION}" -e

# commit CHANGELOG and VERSION updates
git add CHANGELOG.md VERSION
git commit -m "${VERSION} release

[skip ci]"

# merge into release
git checkout release
if ! git diff --exit-code release origin/release
then
  echo "ERROR! There are local-only changes on branch 'release'!"
  exit 1
fi
git merge "prep-release-${VERSION}" -m "Release ${VERSION}"

# tag release
git tag "${VERSION}"

# push to release branch
git push origin release
git push origin "${VERSION}"

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
git push origin master

# delete release-VERSION tag
git push --delete origin "release-${VERSION}"
