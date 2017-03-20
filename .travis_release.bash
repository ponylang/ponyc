#! /bin/bash

set +o errexit
set +o nounset
set +o

here="$(cd "$(dirname "${0}")" && pwd)"
source "${here}/.travis_common.bash"

if [[ "$config" != "release" || "${TRAVIS_OS_NAME}" != "linux" || "${LLVM_CONFIG}" != "llvm-config-3.9" ]]
then
  echo "This is a release branch and there's nothing this matrix element must do."
  exit 0
fi

# set up llvm and pcre2
download-llvm
download-pcre

# install ruby, rpm, and fpm
rvm use 2.2.3 --default
sudo apt-get install -y rpm
gem install fpm

# Build packages for deployment
#
# The PACKAGE_ITERATION will be fed to the DEB and RPM systems by FPM
# as a suffix to the base version (DEB:debian_revision or RPM:release,
# used to disambiguate packages with the same version).
PACKAGE_ITERATION="${TRAVIS_BUILD_NUMBER}.$(git rev-parse --short --verify 'HEAD^{commit}')"
make verbose=1 arch=x86-64 tune=intel config=release package_name="ponyc" package_base_version="$(cat VERSION)" package_iteration="${PACKAGE_ITERATION}" deploy

# Checkout the gh-pages branch
git remote add gh-token "https://${GH_TOKEN}@github.com/${TRAVIS_REPO_SLUG}"
git fetch gh-token && git fetch gh-token gh-pages:gh-pages

# Build documentation
build/release/ponyc packages/stdlib --docs

# Upload documentation on gh-pages
cd stdlib-docs
sudo -H pip install mkdocs
cp ../.docs/extra.js docs/
sed -i '' 's/site_name:\ stdlib/site_name:\ Pony Standard Library/' mkdocs.yml
mkdocs gh-deploy -v --clean --remote-name gh-token
