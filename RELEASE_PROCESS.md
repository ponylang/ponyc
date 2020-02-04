# How to cut a Pony release

This document is aimed at members of the Pony team who might be cutting a release of Pony. It serves as a checklist that can take you through doing a release step-by-step.

## Prerequisites for doing any release

In order to do a release, you absolutely must have:

* Commit access to the `ponyc` repo
* Access to the ponylang twitter account
* An account on the [Pony Zulip](https://ponylang.zulipchat.com)

While not strictly required, you probably be unable to deal with any errors that arise without:

* A [bintray account](https://bintray.com/) and have been granted access `pony-language` organization by a "release admin".
* Read and write access to the ponylang [appveyor](https://www.appveyor.com) account
* Read and write access to the ponylang [cloudsmith](https://cloudsmith.io/) account.

### Validate external services are functional

We rely on both CircleCI and Appveyor as part of our release process. Both need to be up and functional in order to do a release. Check the status of each before starting a release. If either is reporting issues, push the release back a day or until whenever they both are reporting no problems.

* [Appveyor Status](https://appveyor.statuspage.io)
* [Bintray Status](http://status.bintray.com)

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

Leave a comment on the GitHub issue for this release to let everyone know you are done versioning the CHANGELOG and VERSION and that they are updated on `master`.

### "Update" PRs for CHANGELOG

Go through all open PRs and check to see if they are updating the CHANGELOG. If they are, leave a note asking them to merge the latest CHANGELOG into their branch because of the recent versioning for release and to move their additions into the appropriate area of "unreleased".

### Update Homebrew

The [ponyc Homebrew formula](https://github.com/Homebrew/homebrew-core/blob/master/Formula/ponyc.rb) is updated via a GitHub action called: ["Bump Homebrew formula"](https://github.com/ponylang/ponyc/actions?query=workflow%3A%22Release%22).

If for some reason the GitHub action fails, you can use the following instructions to manually update:

Fork the [homebrew-core repo](https://github.com/Homebrew/homebrew-core) and then clone it locally. You are going to be editing "Formula/ponyc.rb". If you already have a local copy of homebrew-core, make sure you sync up with the main Homebrew repo otherwise you might change an older version of the formula and end up with merge conflicts.

Make sure you do your changes on a branch:

* git checkout -b ponyc-0.3.1

HomeBrew has [directions](https://github.com/Homebrew/homebrew-core/blob/master/CONTRIBUTING.md#submit-a-123-version-upgrade-for-the-foo-formula) on what specifically you need to update in a formula to account for an upgrade. If you are on OSX and are unsure of how to get the SHA of the release .tar.gz, download the release file (make sure it does unzip it) and run `shasum -a 256 ponyc-0.3.1.tar.gz`. If you are on OSX, its quite possible it will try to unzip the file on your. In Safari, right clicking and selecting "Download Linked File" will get your the complete .tar.gz.

After updating the ponyc formula, push to your fork and open a PR against homebrew-core. According to the homebrew team, their preferred naming for such PRs is `ponyc 0.3.1` that is, the name of the formula being updated followed by the new version number.

### Update the GitHub issue

Leave a comment on the GitHub issue for this release letting everyone know that the Homebrew formula has been updated and a PR issued. Leave a link to your open PR.

### "Kick off" the Gentoo release

Leave a comment on the GitHub issue for this release letting @stefantalpalaru know that a release is underway. He maintains the Gentoo release. We think it involves magic, but aren't really sure.

### "Kick off" the Nix release

There's no real Nix release. However, @kamilchm is maintaining Nix Pony packages. Drop him a note on the GitHub issue so he is aware of the release.

### "Kick off" the FreeBSD ports update

There's no FreeBSD release that happens. However, @myfreeweb is maintaining the FreeBSD port. Drop a note on the GitHub issue so they are aware of the release.

### Update the GitHub issue as needed

At this point we are basically waiting on Travis, Appveyor and Homebrew. As each finishes, leave a note on the GitHub issue for this release letting everyone know where we stand status wise. For example: "Release 0.3.1 is now available via Homebrew".

### Work on the release notes

We do a blog post announcing each release. The release notes blog post should include highlights of any particularly interesting changes that we want the community to be aware of.

Additionally, any breaking changes that require end users to change their code should be discussed and examples of how to update their code should be included.

[Examples of prior release posts](https://www.ponylang.io/categories/release) are available. If you haven't written release notes before, you should review prior examples to get a feel what should be included. ([example](https://github.com/ponylang/ponylang.github.io/pull/284/files))

### Wait on Appveyor

During the time since you push to the release branch, Appveyor has been busy building the Windows release artifact. This can take up to a couple hours depending on how busy Appveyor is. Periodically check Bintray to see if the release is up yet.

* [Windows](https://bintray.com/pony-language/ponyc-win/ponyc)

The pattern for releases is similar as what we previously saw:

`ponyc-release-0.3.1-1526.8a8ee28`

where the `1526` is the AppVeyor build number and the `8a8ee28` is the abbreviated SHA for the commit we built from.

### Wait on Cloudsmith releases

On each release, we upload ponyc binaries for musl and GNU libc builds to Cloudsmith. The releases are built on [CircleCI](https://circleci.com/gh/ponylang/ponyc).

You can verify that the release artifacts were successfully built and uploaded by checking the [ponylang Cloudsmith releases repository](https://cloudsmith.io/~ponylang/repos/releases/packages/) and verifing that packages exist for both GNU libc and musl libc.

Package names will be:

- ponyc-x86-64-unknown-linux-gnu.tar.gz
- ponyc-x86-64-unknown-linux-musl.tar.gz

and should have a version field listing that matches the current release e.g. `0.3.1`.

In general, you should expect it to take about 30 to 50 minutes to build each package once CircleCI starts running the task.

### Wait on Docker images to be built

As part of every release, 4 Docker images are built:

- Ubuntu images
  - release
  - 0.3.1
- Alpine images
  - release-alpine
  - 0.3.1-alpine

The images are built via GitHub action after Cloudsmith releases have been uploaded. Cloudsmith sends an event to GitHub that triggers Docker images builds in the ["Handle External Events" workflow](https://github.com/ponylang/ponyc/actions?query=workflow%3A%22Handle+external+events%22).You can track the progress of the builds (including failures) there. You can validate that all the images have been pushed by checking the [tag page](https://hub.docker.com/r/ponylang/ponyc/tags) of the [ponyc DockerHub repository](https://hub.docker.com/r/ponylang/ponyc/).

### Verify that the Pony Playground updated to the new version

Once the DockerHub images have been updated, visit the [Pony Playground](https://playground.ponylang.io/) and verify that it has been updated to the correct version by compiling some code and checking the compiler version number in the output.

If it doesn't update automatically, it will need to be done manually. Ping @jemc, @seantallen, @mfelsche, @aturley, @kubali or @theodus.

### Merge the release blog post PR for the ponylang website

Once all the release steps have been confirmed as successful, merge the PR you created earlier for ponylang.github.io for the blog post announcing the release. Confirm it is successfully published to the [blog](https://www.ponylang.io/blog/).

### Inform the Pony Zulip

Once Travis, Appveyor and Homebrew are all finished, drop a note in the [#announce stream](https://ponylang.zulipchat.com/#narrow/stream/189932-announce) with a topic like "0.3.1 has been released" of the Pony Zulip letting everyone know that the release is out and include a link the release blog post.

If this is an "emergency release" that is designed to get a high priority bug fix out, be sure to note that everyone is advised to update ASAP. If the high priority bug only affects certain platforms, adjust the "update ASAP" note accordingly.

### Add to "Last Week in Pony"

Last Week in Pony is our weekly newsletter. Add information about the release, including a link to the release notes, to the [current Last Week in Pony](https://github.com/ponylang/ponylang-website/issues?q=is%3Aissue+is%3Aopen+label%3Alast-week-in-pony). ([example](https://github.com/ponylang/ponylang.github.io/issues/282#issuecomment-392230067))

### Post release notes to Link aggregators

All Pony releases should be posted to:

- [ponylang twitter](https://www.twitter.com/ponylang) ([example](https://twitter.com/ponylang/status/952626693042311169))


### Close the GitHub issue

Close the GitHub issue for this release.
