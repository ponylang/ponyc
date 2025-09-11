# How to cut a Pony release

This document is aimed at members of the Pony team who might be cutting a release of Pony. It serves as a checklist that can take you through doing a release step-by-step.

## Prerequisites

* You must have commit access to the ponyc repository.
* It would be helpful to have read and write access to the ponylang [cloudsmith](https://cloudsmith.io/) account.

### Validate external services are functional

We rely on Cloudsmith and GitHub Actions as part of our release process. Both need to be up and functional in order to do a release. Check the status of each before starting a release. If any are reporting issues, push the release back a day or until whenever they are all reporting no problems.

* [Cloudsmith](https://status.cloudsmith.io/)
* [GitHub](https://www.githubstatus.com/)

## Releasing

Please note that this document was written with the assumption that you are using a clone of the `ponyc` repo. You have to be using a clone rather than a fork. It is advised that you do this by making a fresh clone of the `ponyc` repo from which you will release.

```bash
git clone git@github.com:ponylang/ponyc.git ponyc-release-clean
cd ponyc-release-clean
```

Before getting started, you will need a number for the version that you will be releasing as well as an agreed upon "golden commit" that will form the basis of the release.

The "golden commit" must be `HEAD` on the `main` branch of this repository. At this time, releasing from any other location is not supported.

For the duration of this document, that we are releasing version is `0.3.1`. Any place you see those values, please substitute your own version.

```bash
git tag release-0.3.1
git push origin release-0.3.1
```

### Wait on release artifacts

On each release, we upload ponyc binaries for builds to Cloudsmith. The releases are built on [GitHub Actions](https://github.com/ponylang/ponyc/actions/workflows/release.yml).

You can verify that the release artifacts were successfully built and uploaded by checking the [ponylang Cloudsmith releases repository](https://cloudsmith.io/~ponylang/repos/releases/packages/) and that all packages exist.

Package names will be:

* ponyc-arm64-apple-darwin.tar.gz
* ponyc-arm64-unknown-linux-alpine3.21.tar.gz
* ponyc-arm64-unknown-linux-ubuntu24.04.tar.gz
* ponyc-x86-64-apple-darwin.tar.gz
* ponyc-x86-64-pc-windows-msvc.zip
* ponyc-x86-64-unknown-linux-alpine3.20.tar.gz
* ponyc-x86-64-unknown-linux-alpine3.21.tar.gz
* ponyc-x86-64-unknown-linux-fedora41.tar.gz
* ponyc-x86-64-unknown-linux-musl.tar.gz
* ponyc-x86-64-unknown-linux-ubuntu22.04.tar.gz
* ponyc-x86-64-unknown-linux-ubuntu24.04.tar.gz

and should have a version field listing that matches the current release e.g. `0.3.1`.

If not all files are presents after 10 to 15 minutes then either a build job is delayed waiting for resources or something has gone wrong. Check [GitHub Actions](https://github.com/ponylang/ponyc/actions/workflows/release.yml) to learn more.

### Wait on Docker images to be built

As part of every release, 6 Docker images are built:

* Ubuntu images
  * release
  * 0.3.1
* Alpine images
  * release-alpine
  * 0.3.1-alpine
* Windows images
  * release-windows
  * 0.3.1-windows

The images are built via GitHub action after Linux releases have been uploaded to Cloudsmith. Cloudsmith sends an event to GitHub that triggers Docker images builds in the ["Cloudsmith package synchronised" workflow](https://github.com/ponylang/ponyc/actions/workflows/cloudsmith-package-sychronised.yml).You can track the progress of the builds (including failures) there. You can validate that all the images have been pushed by checking the tags of the [ponylang/ponyc package](https://github.com/ponylang/ponyc/pkgs/container/ponyc).

### Verify that the Pony Playground updated to the new version

Once the images have been updated, the Pony playground should automatically update. You can check the version running by compiling a small Pony program and verifying that the version listed in the output matches the newly released Pony version.

If the versions are different, you'll need to log in to the playground server as root and run `bash update-playground.bash`.

## If something goes wrong

The release process can be restarted at various points in it's lifecycle by pushing specially crafted tags.

### Start a release

As documented above, a release is started by pushing a tag of the form `release-x.y.z`.

### Build artifacts

The release process can be manually restarted from here by pushing a tag of the form `x.y.z`. The pushed tag must be on the commit to build the release artifacts from. During the normal process, that commit is the same as the one that `release-x.y.z`.

### Announce release

The release process can be manually restarted from here by push a tag of the form `announce-x.y.z`. The tag must be on a commit that is after "Release x.y.z" commit that was generated during the `Start a release` portion of the process.

If you need to restart from here, you will need to pull the latest updates from the ponyc repo as it will have changed and the commit you need to tag will not be available in your copy of the repo with pulling.
