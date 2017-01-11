#!/bin/bash

release_date=`date +%Y-%m-%d`

verify_args() {
  echo "Cutting a release for version $version with commit $commit"
  while true; do
    read -p "Is this correct (y/n)?" yn
    case $yn in
      [Yy]* ) break;;
      [Nn]* ) exit;;
      * ) echo "Please answer y or n.";;
    esac
  done
}

update_version() {
  > VERSION
  echo $version >> VERSION
  echo "VERSION set to $version"
}

update_changelog() {
  # cut out empty "Fixed", "Added", or "Changed" sections
  remove_empty_sections

  local match='## \[unreleased\] \- unreleased'
  local heading="## [$version] - $release_date"
  sed -i "/$match/c $heading" CHANGELOG.md
}

remove_empty_sections() {
  for section in "Fixed" "Added" "Changed"; do
    # check if first non-whitespace character after section heading is '-'
    local str_after=`sed "1,/### $section/d" <CHANGELOG.md`
    local first_word=`echo $str_after | head -n1 | cut -d " " -f1`
    if [ $first_word != "-" ]; then
      echo "Removing empty CHANGELOG section: $section"
      sed -i "0,/### $section/{//d;}" CHANGELOG.md
    fi
  done
}

add_changelog_unreleased_section() {
  echo "Adding unreleased section to CHANGELOG"
  local replace='## [unreleased] - unreleased\n\n### Fixed\n\n### Added\n\n### Changed\n\n'
  sed -i "/## \[$version\] \- $release_date/i $replace" CHANGELOG.md
}


if [ $# -le 2 ]; then
  echo "version and commit arguments required"
fi

set -eu
version=$1
commit=$2

verify_args

# create version release branch
git checkout master
git pull
git checkout -b release-$version $commit

# update VERSION and CHANGELOG
update_version
update_changelog

# commit VERSION and CHANGELOG updates
git add CHANGELOG.md VERSION
git commit -m "Prep for $version release"

# merge into release
git checkout release
git merge release-$version

# tag release
git tag $version

# push to release branch
git push origin release
git push origin $version

# update CHANGELOG for new entries
git checkout master
git merge release-$version
add_changelog_unreleased_section

# commit changelog and push to master
git add CHANGELOG.md
git commit -m "Add unreleased section to CHANGELOG post $version release prep"
git push origin master
