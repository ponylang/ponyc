#! /bin/bash

set -o errexit
set -o nounset

download_vagrant(){
  echo "Downloading and installing vagrant/libvirt..."
  sudo add-apt-repository ppa:linuxsimba/libvirt-udp-tunnel -y
  sudo apt-get update
  sudo apt-get install libvirt-bin libvirt-dev qemu-utils qemu -y
  sudo /etc/init.d/libvirt-bin restart
  sudo virsh pool-define-as --name default --type dir --target /var/lib/libvirt/images
  sudo virsh pool-autostart default || true
  sudo virsh pool-build default || true
  sudo virsh pool-start default || true
  sudo /etc/init.d/libvirt-bin restart
  sudo libvirtd --version
  wget "https://releases.hashicorp.com/vagrant/2.1.2/vagrant_2.1.2_x86_64.deb"
  sudo dpkg -i vagrant_2.1.2_x86_64.deb
  rm vagrant_2.1.2_x86_64.deb
  vagrant plugin install vagrant-libvirt
  cd .ci-vagrantfiles/${VAGRANT_ENV}
  sudo vagrant up --provider=libvirt
}

osx-ponyc-test(){
  echo "Building and testing ponyc..."
  make CC="$CC1" CXX="$CXX1" -j$(sysctl -n hw.ncpu) all
  make CC="$CC1" CXX="$CXX1" test-ci
}

build_appimage(){
  package_version=$1

  mkdir ponyc.AppDir

  cat > ./ponyc.desktop <<\EOF
[Desktop Entry]
Name=Pony Compiler
Icon=ponyc
Type=Application
NoDisplay=true
Exec=ponyc
Terminal=true
Categories=Development;
EOF

  curl https://www.ponylang.io/images/logo.png -o ponyc.png

  curl https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -o linuxdeploy-x86_64.AppImage -J -L
  chmod +x linuxdeploy-x86_64.AppImage

  # remove any existing build artifacts
  sudo rm -rf build

  # can't run appimages in docker; need to extract and then run
  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "./linuxdeploy-x86_64.AppImage --appimage-extract"

  # need to run in CentOS 7 docker image
  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "make LLVM_CONFIG=/opt/llvm-3.9.1/bin/llvm-config arch=x86-64 tune=generic default_pic=true use=llvm_link_static DESTDIR=ponyc.AppDir ponydir=/usr prefix= test-ci"
  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "make LLVM_CONFIG=/opt/llvm-3.9.1/bin/llvm-config arch=x86-64 tune=generic default_pic=true use=llvm_link_static DESTDIR=ponyc.AppDir ponydir=/usr prefix= install"

  # build stdlib to ensure libraries like openssl and pcre2 get packaged in the appimage
  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "./build/release/ponyc packages/stdlib"
  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "cp stdlib ponyc.AppDir/usr/bin"

  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "ARCH=x86_64 ./squashfs-root/AppRun --appdir ponyc.AppDir --desktop-file=ponyc.desktop --icon-file=ponyc.png"

  # delete stdlib to not include it in appimage
  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "rm ponyc.AppDir/usr/bin/stdlib"

  # build appimage
  sudo docker run -v "$(pwd):/home/pony" -u pony:2000 --rm ponylang/ponyc-ci:centos7-llvm-3.9.1 sh -c "ARCH=x86_64 ./squashfs-root/AppRun --appdir ponyc.AppDir --desktop-file=ponyc.desktop --icon-file=ponyc.png --output appimage"

  mv Pony_Compiler-x86_64.AppImage "Pony_Compiler-${package_version}-x86_64.AppImage"

  bash .bintray.bash appimage "${package_version}" ponyc

  rm linuxdeploy-x86_64.AppImage

  # cleanup
  sudo rm -rf build
}

build_deb(){
  deb_distro=$1

  # untar source code
  #tar -xzf "ponyc_${package_version}.orig.tar.gz"
  git clone --depth 1 --branch "$package_version" --recurse-submodules --shallow-submodules https://github.com/ponylang/ponyc.git

  #pushd ponyc-*
  pushd ponyc

  cp -r .packaging/deb debian
  cp LICENSE debian/copyright

  # create changelog
  rm -f debian/changelog
  dch --package ponyc -v "${package_version}" -D "${deb_distro}" --force-distribution --controlmaint --create "Release ${package_version}"

  # create package for distro using docker to run debuild
  sudo docker run -v "$(pwd)/..:/home/pony" --rm --user root "ponylang/ponyc-ci:${deb_distro}-deb-builder-lib-llvm" sh -c 'cd ponyc && apt-get update && mk-build-deps -t "apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends -y" -i -r && debuild -b -us -uc'

  ls -l ..

  # restore original working directory
  popd

  # create bintray upload json file
  bash .bintray_deb.bash "$package_version" "$deb_distro"

  # rename package to avoid clashing across different distros packages
  mv "ponyc_${package_version}_amd64.deb" "ponyc_${package_version}_${deb_distro}_amd64.deb"

  # clean up old build directory to ensure things are all clean
  #sudo rm -rf ponyc-*
  sudo rm -rf ponyc
}

ponyc-build-debs-ubuntu(){
  package_version=$1

  set -x

  echo "Install devscripts..."
  sudo apt-get update
  sudo apt-get install -y devscripts

  echo "Building off ponyc debs for bintray..."
  #wget "https://github.com/ponylang/ponyc/archive/${package_version}.tar.gz" -O "ponyc_${package_version}.orig.tar.gz"

  #if [ "${package_version}" == "test-building-apt-packages-with-vendored-llvm" ]
  #then
  #  mv "ponyc_${package_version}.orig.tar.gz" "ponyc_$(cat VERSION).orig.tar.gz"
  #  package_version=$(cat VERSION)
  #fi

  # To add a new ubuntu version
  # * Create and upload the appropriate docker image using .ci-dockerfiles/deb-builder templated Docker file
  # * Add a new entry to the following list calling `build_deb` for the new distribution
  #
  # To remove a existing ubuntu version
  # * Remove the exiting entry from the following list calling `build_deb` for the retired distribution
  #
  # To update the builder image for an existing ubuntu version
  # * Create and upload a new version of the appropriate docker image using .ci-dockerfiles/deb-builder
  build_deb xenial
  #build_deb bionic

  ls -la
  set +x
}

ponyc-build-debs-debian(){
  package_version=$1

  set -x

  echo "Install devscripts..."
  sudo apt-get update
  sudo apt-get install -y devscripts

  echo "Building off ponyc debs for bintray..."
  #wget "https://github.com/ponylang/ponyc/archive/${package_version}.tar.gz" -O "ponyc_${package_version}.orig.tar.gz"

  #if [ "${package_version}" == "test-building-apt-packages-with-vendored-llvm" ]
  #then
  #  mv "ponyc_${package_version}.orig.tar.gz" "ponyc_$(cat VERSION).orig.tar.gz"
  #  package_version=$(cat VERSION)
  #fi

  # To add a new debian version
  # * Create and upload the appropriate docker image using .ci-dockerfiles/deb-builder templated Docker file
  # * Add a new entry to the following list calling `build_deb` for the new distribution
  #
  # To remove a existing debian version
  # * Remove the exiting entry from the following list calling `build_deb` for the retired distribution
  #
  # To update the builder image for an existing debian version
  # * Create and upload a new version of the appropriate docker image using .ci-dockerfiles/deb-builder
  build_deb buster
  #build_deb stretch

  ls -la
  set +x
}

ponyc-kickoff-copr(){
  package_version=$(cat VERSION)

  # COPR for fedora/centos/suse
  echo "Kicking off ponyc packaging for COPR..."
  docker run -it --rm -e COPR_LOGIN="${COPR_LOGIN}" -e COPR_USERNAME=ponylang -e COPR_TOKEN="${COPR_TOKEN}" -e COPR_COPR_URL=https://copr.fedorainfracloud.org mgruener/copr-cli buildscm --clone-url https://github.com/ponylang/ponyc --commit "${package_version}" --subdir /.packaging/rpm/ --spec ponyc.spec --type git --nowait ponylang
}

ponyc-build-packages(){
  echo "Installing ruby, rpm, and fpm..."
  rvm use 2.2.3 --default
  sudo apt-get install -y rpm
  gem install fpm

  echo "Building ponyc packages for deployment..."
  make CC="$CC1" CXX="$CXX1" verbose=1 arch=x86-64 tune=intel package_name="ponyc" package_base_version="$(cat VERSION)" deploy
}

ponyc-build-docs(){
  echo "Installing mkdocs and offical theme..."
  pip3 install --user mkdocs-ponylang
  echo "Building ponyc docs..."
  make CC="$CC1" CXX="$CXX1" docs-online
}

ponyc-upload-docs(){
  echo "Uploading docs using mkdocs..."
  git remote add gh-token "https://${STDLIB_TOKEN}@github.com/ponylang/stdlib.ponylang.io"
  git fetch gh-token
  git reset gh-token/master
  pushd stdlib-docs
  mkdocs gh-deploy -v --clean --remote-name gh-token --remote-branch master
  popd
}
