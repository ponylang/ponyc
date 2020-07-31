# How to cut a Pony release

This document is aimed at members of the Pony team who might be cutting a release of Pony. It serves as a checklist that can take you through doing a release step-by-step.

## Prerequisites for doing any release

In order to do a release, you absolutely must have:

* Commit access to the `ponyc` repo
* Access to the ponylang twitter account
* An account on the [Pony Zulip](https://ponylang.zulipchat.com)

While not strictly required, you probably be unable to deal with any errors that arise without:

* Read and write access to the ponylang [cloudsmith](https://cloudsmith.io/) account.

### Validate external services are functional

We rely on CirrusCI, Cloudsmith, DockerHub, and GitHub Actions as part of our release process. All  need to be up and functional in order to do a release. Check the status of each before starting a release. If any are reporting issues, push the release back a day or until whenever they are all reporting no problems.

* [CirrusCI Status](https://twitter.com/cirrus_labs)
* [Cloudsmith](https://status.cloudsmith.io/)
* [DockerHub](https://status.docker.com/)
* [GitHub](https://www.githubstatus.com/)

### A GitHub issue exists for the release

All releases should have a corresponding issue in the [ponyc repo](https://github.com/ponylang/ponyc/issues). This issue should be used to communicate the progress of the release (updated as checklist steps are completed). You can also use the issue to notify other pony team members of problems.

## Releasing

Please note that this document was written with the assumption that you are using a clone of the `ponyc` repo. You have to be using a clone rather than a fork. It is advised that you do this by making a fresh clone of the `ponyc` repo from which you will release.

```bash
git clone git@github.com:ponylang/ponyc.git ponyc-release-clean
cd ponyc-release-clean
```

Before getting started, you will need a number for the version that you will be releasing as well as an agreed upon "golden commit" that will form the basis of the release.

The "golden commit" must be `HEAD` on the `master` branch of this repository. At this time, releasing from any other location is not supported.

For the duration of this document, that we are releasing version is `0.3.1`. Any place you see those values, please substitute your own version.

```bash
git tag release-0.3.1
git push origin release-0.3.1
```

### Update the GitHub issue

Leave a comment on the GitHub issue that the release process has been started.

### "Kick off" the Gentoo release

Leave a comment on the GitHub issue for this release letting @stefantalpalaru know that a release is underway. He maintains the Gentoo release. We think it involves magic, but aren't really sure.

### Work on the release notes

We do release notes for each release. The release notes should include highlights of any particularly interesting changes that we want the community to be aware of.

Additionally, any breaking changes that require end users to change their code should be discussed and examples of how to update their code should be included.

[Examples of prior release posts](https://github.com/ponylang/ponyc/releases/) are available. If you haven't written release notes before, you should review prior examples to get a feel what should be included. ([example](https://github.com/ponylang/ponyc/releases/tag/0.35.0))

### Wait on release artifacts

On each release, we upload ponyc binaries for builds to Cloudsmith. The releases are built on [CirrusCI](https://cirrus-ci.com/github/ponylang/ponyc).

You can verify that the release artifacts were successfully built and uploaded by checking the [ponylang Cloudsmith releases repository](https://cloudsmith.io/~ponylang/repos/releases/packages/) and that all packages exist.

Package names will be:

- ponyc-x86-64-apple-darwin.tar.gz
- ponyc-x86-64-unknown-freebsd-12.1.tar.gz
- ponyc-x86-64-pc-windows-msvc.zip
- ponyc-x86-64-unknown-linux-gnu.tar.gz
- ponyc-x86-64-unknown-linux-musl.tar.gz
- ponyc-x86-64-unknown-linux-ubuntu18.04.tar.gz

and should have a version field listing that matches the current release e.g. `0.3.1`.

If not all files are presents after 10 to 15 minutes then either a build job is delayed waiting for resources or something has gone wrong. Check [CirrusCI](https://cirrus-ci.com/github/ponylang/ponyc) to learn more.

### Wait on Docker images to be built

As part of every release, 4 Docker images are built:

- Ubuntu images
  - release
  - 0.3.1
- Alpine images
  - release-alpine
  - 0.3.1-alpine

The images are built via GitHub action after Linux releases have been uploaded to Cloudsmith. Cloudsmith sends an event to GitHub that triggers Docker images builds in the ["Handle External Events" workflow](https://github.com/ponylang/ponyc/actions?query=workflow%3A%22Handle+external+events%22).You can track the progress of the builds (including failures) there. You can validate that all the images have been pushed by checking the [tag page](https://hub.docker.com/r/ponylang/ponyc/tags) of the [ponyc DockerHub repository](https://hub.docker.com/r/ponylang/ponyc/).

### Verify that the Pony Playground updated to the new version

Once the DockerHub images have been updated, the Pony playground will need to be updated. If you don't have access to the playground server, ping @jemc, @seantallen, @mfelsche, @aturley, @kubali or @theodus and let them know the update needs to be done.

To update, you currently must do the following after logging in:

```bash
systemctl stop playground
cd pony-playground
docker build docker --pull -t ponylang-playpen
systemctl start playground
```

### Inform the Pony Zulip

Once CirrusCI, Cloudsmith, and GitHub Actions are all finished, drop a note in the [#announce stream](https://ponylang.zulipchat.com/#narrow/stream/189932-announce) with a topic like "0.3.1 has been released" of the Pony Zulip letting everyone know that the release is out and include a link the release blog post.

If this is an "emergency release" that is designed to get a high priority bug fix out, be sure to note that everyone is advised to update ASAP. If the high priority bug only affects certain platforms, adjust the "update ASAP" note accordingly.

### Add to "Last Week in Pony"

Last Week in Pony is our weekly newsletter. Add information about the release, including a link to the release notes, to the [current Last Week in Pony](https://github.com/ponylang/ponylang-website/issues?q=is%3Aissue+is%3Aopen+label%3Alast-week-in-pony). ([example](https://github.com/ponylang/ponylang.github.io/issues/282#issuecomment-392230067))

### Announce on Twitter

All Pony releases should be posted to:

- [ponylang twitter](https://www.twitter.com/ponylang) ([example](https://twitter.com/ponylang/status/952626693042311169))


### Close the GitHub issue

Close the GitHub issue for this release.
