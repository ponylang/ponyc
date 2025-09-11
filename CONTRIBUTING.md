# Contributing

It's good to hear that you want to contribute to Pony!

There are a number of ways to contribute to Pony. As this document is a little long, feel free to jump to the section that applies to you currently:

* [Feature request](#feature-request)
* [Bug report](#bug-report)
* [How to contribute](#how-to-contribute)
* [Pull request](#pull-request)

Additional notes regarding formatting:

* [Documentation formatting](#documentation-formatting)
* [Code formatting](#code-formatting)
* [Standard Library File Naming](#standard-library-file-naming)

## Feature request

For any feature requests or enhancements to the Pony distribution, it is quite likely that you have to go through our [RFC process](https://github.com/ponylang/rfcs). Before opening or submitting any feature requests, please make sure you are familiar with the RFC process and follow the process as required.

If you submit a pull request to implement a new feature without going through the RFC process, it may be closed with a polite request to submit an RFC first.

## Bug report

First of all please [search existing issues](https://github.com/ponylang/ponyc/issues) to make sure your issue hasn't already been reported. If you cannot find a suitable issue — [create a new one](https://github.com/ponylang/ponyc/issues/new).

Provide the following details:

* short summary of what you was trying to achieve,
* a code causing the bug,
* expected result,
* actual results and
* environment details: at least operating system and compiler version (`ponyc -v`).

If possible, try to isolate the problem and provide just enough code to demonstrate it. Add any related information which might help to fix the issue.

## How to contribute

We use a fairly standard GitHub pull request workflow. If you have already contributed to a project via GitHub pull request, you can skip this section and proceed to the [specific details of what we ask for in a pull request](#pull-request). If this is your first time contributing to a project via GitHub, read on.

Here is the basic GitHub workflow:

1. Fork the ponyc repo. you can do this via the GitHub website. This will result in you having your own copy of the ponyc repo under your GitHub account.
2. Clone your ponyc repo to your local machine
3. Make a branch for your change
4. Make your change on that branch
5. Push your change to your repo
6. Use the github ui to open a PR

Some things to note that aren't immediately obvious to folks just starting out:

1. Your fork doesn't automatically stay up to date with change in the main repo.
2. Any changes you make on your branch that you used for the PR will automatically appear in the PR so if you have more than 1 PR, be sure to always create different branches for them.
3. Weird things happen with commit history if you don't create your PR branches off of main so always make sure you have the main branch checked out before creating a branch for a PR

If you feel overwhelmed at any point, don't worry, it can be a lot to learn when you get started. Feel free to reach out via [Zulip](https://ponylang.zulipchat.com/#narrow/stream/192795-contribute-to-Pony).

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

## Grammar Changes

If your contribution contains changes to the grammar of the language, you should update the `pony.g` file at the root of the repository with `ponyc --antlr > pony.g`.

## Documentation Formatting

When contributing to documentation, try to keep the following style guidelines in mind:

* Wherever possible, try to match the style of surrounding documentation.
* Avoid hard-wrapping lines within paragraphs (using line breaks in the middle of or between sentences to make lines shorter than a certain length). Instead, turn on soft-wrapping in your editor and expect the documentation renderer to let the text flow to the width of the container.
* Apply code highlighting to identifier names, such as types, fields, variables and parameters via the customary markdown syntax of wrapping the name in backticks.

## Code Formatting

For code formatting guidelines please see [The Style Guide](https://github.com/ponylang/ponyc/blob/main/STYLE_GUIDE.md).

## Standard Library File Naming

For standard library file naming guidelines see [The Style Guide](https://github.com/ponylang/ponyc/blob/main/STYLE_GUIDE.md#naming).

## Source Code Coverage of Ponyc

To get C code coverage information for test runs or for calling ponyc, call `make` with `use=coverage config=debug`. This works both for *clang* and *gcc*. Make sure to configure `CC` and `CXX` environment variables both to either `gcc` and `g++` or `clang` and `clang++`.

### Using gcc and lcov

* Compile ponyc with `use=coverage config=debug` and environment variables `CC=gcc CXX=g++`
* Run ponyc or the test suite from `build/debug-coverage`
* generate the html coverage report:

  ```bash
  # generate coverage report
  lcov --directory .build/debug-coverage/obj –zerocounters
  lcov --directory .build/debug-coverage/obj --capture --output-file ponyc.info
  genhtml -o build/debug-coverage/coverage ponyc.info
  ```

* open the html report at `build/debug-coverage/coverage/index.html`

### Using clang and llvm-cov

* Compile ponyc with `use=coverage config=debug` and environment variables `CC=clang CXX=clang++`
* Run ponyc or the test suite from `build/debug-coverage` with environment variable: `LLVM_PROFILE_FILE="build/debug-coverage/coverage.profraw"`
* generate coverage data:

  ```bash
  llvm-profdata merge -sparse -output=build/debug-coverage/coverage.profdata build/debug-coverage/coverage.profraw
  ```

* show coverage data (only for `lexer.c` in this case):

  ```bash
  llvm-cov show ./build/debug-coverage/libponyc.tests -instr-profile=./build/debug-coverage/coverage.profdata src/libponyc/ast/lexer.c
  ```
