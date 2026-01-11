# Contributing

You want to contribute to peg? Awesome.

There are a number of ways to contribute. As this document is a little long, feel free to jump to the section that applies to you currently:

* [Bug report](#bug-report)
* [How to contribute](#how-to-contribute)
* [Pull request](#pull-request)

Additional notes regarding formatting:

* [Documentation formatting](#documentation-formatting)
* [Code formatting](#code-formatting)
* [File naming](#file-naming)

## Bug report

First of all please [search existing issues](https://github.com/ponylang/peg/issues) to make sure your issue hasn't already been reported. If you cannot find a suitable issue — [create a new one](https://github.com/ponylang/peg/issues/new).

Provide the following details:

* short summary of what you were trying to achieve,
* a code snippet causing the bug,
* expected result,
* actual results and
* environment details: at least operating system version

If possible, try to isolate the problem and provide just enough code to demonstrate it. Add any related information which might help to fix the issue.

## How to contribute

This project uses a fairly standard GitHub pull request workflow. If you have already contributed to a project via GitHub pull request, you can skip this section and proceed to the [specific details of what we ask for in a pull request](#pull-request). If this is your first time contributing to a project via GitHub, read on.

Here is the basic GitHub workflow:

1. Fork this repo. you can do this via the GitHub website. This will result in you having your own copy of the repo under your GitHub account.
2. Clone your forked repo to your local machine
3. Make a branch for your change
4. Make your change on that branch
5. Push your change to your repo
6. Use the github ui to open a PR

Some things to note that aren't immediately obvious to folks just starting out:

1. Your fork doesn't automatically stay up to date with changes in the main repo.
2. Any changes you make on your branch that you used for one PR will automatically appear in another PR so if you have more than 1 PR, be sure to always create different branches for them.
3. Weird things happen with commit history if you don't create your PR branches off of `main` so always make sure you have the `main` branch checked out before creating a branch for a PR

You can get help using GitHub via [the official documentation](https://help.github.com/). Some highlights include:

* [Fork A Repo](https://help.github.com/articles/fork-a-repo/)
* [Creating a pull request](https://help.github.com/articles/creating-a-pull-request/)
* [Syncing a fork](https://help.github.com/articles/syncing-a-fork/)

## Pull request

While we don't require that your pull request be a single commit, note that we will end up squashing all your commits into a single commit when we merge. While your PR is in review, we may ask for additional changes, please do not squash those commits while the review is underway. We ask that you not squash while a review is underway as it can make it hard to follow what is going on.

When opening your pull request, please make sure that the initial comment on the PR is the commit message we should use when we merge. Making sure your commit message conforms to these guidelines for [writ(ing) a good commit message](http://chris.beams.io/posts/git-commit/).

Make sure to issue 1 pull request per feature. Don't lump unrelated changes together. If you find yourself using the word "and" in your commit comment, you
are probably doing too much for a single PR.

We keep a [CHANGELOG](CHANGELOG.md) of all software changes with behavioural effects in ponyc. If your PR includes such changes (rather than say a documentation update), a Pony team member will do the following before merging it, so that the PR will be automatically added to the CHANGELOG:

* Ensure that the ticket is tagged with one or more appropriate "changelog - *" labels - each label corresponds to a section of the changelog where this change will be automatically mentioned.
* Ensure that the ticket title is appropriate - the title will be used as the summary of the change, so it should be appropriately formatted, including a ticket reference if the PR is a fix to an existing bug ticket.
  * For example, an appropriate title for a PR that fixes a bug reported in issue ticket #98 might look like:
  * *Fixed compiler crash related to tuple recovery (issue #98)*

Once those conditions are met, the PR can be merged, and an automated system will immediately add the entry to the changelog. Keeping the changelog entries out of the file changes in the PR helps to avoid conflicts and other administrative headaches when many PRs are in progress.

Any change that involves a changelog entry will trigger a bot to request that you add release notes to your PR.

Pull requests from accounts that aren't members of the Ponylang organization require approval from a member before running. Approval is required after each update that you make. This could involve a lot of waiting on your part for approvals. If you are opening PRs to verify that changes all pass CI before "opening it for real", we strongly suggest that you open the PR against the `main` branch of your fork. CI will then run in your fork and you don't need to wait for approval from a Ponylang member.

## Documentation formatting

When contributing to documentation, try to keep the following style guidelines in mind:

As much as possible all documentation should be textual and in Markdown format. Diagrams are often needed to clarify a point. For any images, an original high-resolution source should be provided as well so updates can be made.

Documentation is not "source code." As such, it should not be wrapped at 80 columns. Documentation should be allowed to flow naturally until the end of a paragraph. It is expected that the reader will turn on soft wrapping as needed.

All code examples in documentation should be formatted in a fashion appropriate to the language in question.

All command line examples in documentation should be presented in a copy and paste friendly fashion. Assume the user is using the `bash` shell. GitHub formatting on long command lines can be unfriendly to copy-and-paste. Long command lines should be broken up using `\` so that each line is no more than 80 columns. Wrapping at 80 columns should result in a good display experience in GitHub. Additionally, continuation lines should be indented two spaces.

OK:

```bash
my_command --some-option foo --path-to-file ../../project/long/line/foo \
  --some-other-option bar
```

Not OK:

```bash
my_command --some-option foo --path-to-file ../../project/long/line/foo --some-other-option bar
```

Wherever possible when writing documentation, favor full command options rather than short versions. Full flags are usually much easier to modify because the meaning is clearer.

OK:

```bash
my_command --messages 100
```

Not OK:

```bash
my_command -m 100
```

## Code formatting

The basics:

* Indentation

Indent using spaces, not tabs. Indentation is language specific.

* Watch your whitespace!

Use an editor plugin to remove unused trailing whitespace including both at the end of a line and at the end of a file. By the same token, remember to leave a single newline only line at the end of each file. It makes output files to the console much more pleasant.

* Line Length

We all have different sized monitors. What might look good on yours might look like awful on another. Be kind and wrap all lines at 80 columns unless you have a good reason not to.

* Reformatting code to meet standards

Try to avoid doing it. A commit that changes the formatting for large chunks of a file makes for an ugly commit history when looking for changes. Don't commit code that doesn't conform to coding standards in the first place. If you do reformat code, make sure it is either standalone reformatting with no logic changes or confined solely to code whose logic you touched. For example, updating the indentation in a file? Do not make logic changes along with it. Editing a line that has extra whitespace at the end? Feel free to remove it.

The details:

All Pony sources should follow the [Pony standard library style guide](https://github.com/ponylang/ponyc/blob/main/STYLE_GUIDE.md).

## File naming

Pony code follows the [Pony standard library file naming guidelines](https://github.com/ponylang/ponyc/blob/main/STYLE_GUIDE.md#naming).
