Contributing
============

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

Feature request
---------------
For any feature requests or enhancements to the Pony distribution, it is quite likely that you have to go through our [RFC process](https://github.com/ponylang/rfcs). Before opening or submitting any feature requests, please make sure you are familiar with the RFC process and follow the process as required.

If you submit a pull request to implement a new feature without going through the RFC process, it may be closed with a polite request to submit an RFC first.

Bug report
----------
First of all please [search existing issues][complete-issue-list] to make sure your issue hasn't already been reported. If you cannot find a suitable issue — [create a new one][new-issue].

Provide the following details:

  - short summary of what you was trying to achieve,
  - a code causing the bug,
  - expected result,
  - actual results and
  - environment details: at least operating system and compiler version (`ponyc -v`).

If possible, try to isolate the problem and provide just enough code to demonstrate it. Add any related information which might help to fix the issue.

How to contribute
-----------------

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
2. Any changes you make on your branch that you used for the PR will automatically appear in the PR so if you have more than 1 PR, be sure to always create different braches for them.
3. Weird things happen with commit history if you dont create your PR branches off of master so always make sure you have the master branch checked out before creating a branch for a PR

If you feel overwhelmed at any point, don't worry, it can be a lot to learn when you get started. Feel free to reach out via [IRC](https://webchat.freenode.net/?channels=%23ponylang) or the [pony developer mailing list](https://pony.groups.io/g/dev) for assistance.

You can get help using GitHub via [the official documentation](https://help.github.com/). Some hightlights include:

- [Fork A Repo](https://help.github.com/articles/fork-a-repo/)
- [Creating a pull request](https://help.github.com/articles/creating-a-pull-request/)
- [Syncing a fork](https://help.github.com/articles/syncing-a-fork/)

Pull request
------------
Before issuing a pull request we ask that you squash all your commits into a single logical commit. While your PR is in review, we may ask for additional changes, please do not squash those commits while the review is underway. Once everything is good, we'll then ask you to further squash those commits before merging. We ask that you not squash while a review is underway as it can make it hard to follow what is going on. Additionally, we ask that you:

* [Write a good commit message](http://chris.beams.io/posts/git-commit/)
* Issue 1 Pull Request per feature. Don't lump unrelated changes together.

If you aren't sure how to squash multiple commits into one, Steve Klabnik wrote [a handy guide](http://blog.steveklabnik.com/posts/2012-11-08-how-to-squash-commits-in-a-github-pull-request) that you can refer to.

We keep a [CHANGELOG](CHANGELOG.md) of all software changes with behavioural effects in ponyc. If your PR includes such changes (rather than say a documentation update), a Pony team member will do the following before merging it, so that the PR will be automatically added to the CHANGELOG:

* Ensure that the ticket is tagged with one or more appropriate "changelog - *" labels - each label corresponds to a section of the changelog where this change will be automatically mentioned.
* Ensure that the ticket title is appropriate - the title will be used as the summary of the change, so it should be appropriately formatted, including a ticket reference if the PR is a fix to an existing bug ticket.
  * For example, an appropriate title for a PR that fixes a bug reported in issue ticket #98 might look like:
  * *Fixed compiler crash related to tuple recovery (issue #98)*

Once those conditions are met, the PR can be merged, and an automated system will immediately add the entry to the changelog. Keeping the changelog entries out of the file changes in the PR helps to avoid conflicts and other administrative headaches when many PRs are in progress.

Please note, if your changes are purely to things like README, CHANGELOG etc, you can add [skip ci] as the last line of your commit message and your PR won't be run through our continuous integration systems. We ask that you use [skip ci] where appropriate as it helps to get changes through CI faster and doesn't waste resources that Appveyor and TravisCI are kindly donating to the Open Source community.

Documentation Formatting
---------------
When contributing to documentation, try to keep the following style guidelines in mind:

* Wherever possible, try to match the style of surrounding documentation.
* Avoid hard-wrapping lines within paragraphs (using line breaks in the middle of or between sentences to make lines shorter than a certain length). Instead, turn on soft-wrapping in your editor and expect the documentation renderer to let the text flow to the width of the container.
* Apply code highlighting to identifier names, such as types, fields, variables and parameters via the customary markdown syntax of wrapping the name in backticks.  

Code Formatting
---------------
A few code formatting rules to be aware of if you are submitting changes. In general, take a look at the existing code and try to match that formatting style. A couple things that might not be immediately obvious.

* Indentation

We indent using spaces not tabs. The unit of indentation is two spaces.

* Watch your whitespace!

Use an editor plugin to remove unused trailing whitespace. This includes both at the end of a line and at the end of a file. By the same token, remember to leave a single newline only line at the end of each file. It makes output files to the console much more pleasant.

* Line Length

We all have different sized monitors. What might look good on yours might look like awful on another. Be kind and wrap all lines at 80 columns unless you have a good reason not to.

* Style

*Variable and method names* -- Variable and method names should be lowercase and should have underscores between the words. Examples: `from_array`, `offset_to_index`.

*Class, type, interface, trait, and primitive names* -- Class, type, interface, trait, and primitive names should start with an uppercase letter and each word should be capitalized. Examples: `StringBytes`, `ErrorReason`, `ReadlineNotify`, `FloatingPoint` `UnrecognizedOption`.

*Breaking up parameter lists* -- A method signature may need to be split across several lines in order to follow the rule that the line length must be 80 characters or fewer. The accepted way to break up a parameter list is to indent all lines after the first line by 2 spaces, and to place the `=>` on a line by itself at the same level of indentation as the `fun` or `be` at the beginning of the declaration. Example:

```pony
  fun ref append(seq: (ReadSeq[A] & ReadElement[A^]), offset: USize = 0,
    len: USize = -1): Array[A]^
  =>
```

* Reformatting code to meet standards

Try to avoid doing it. A commit that changes the formatting for large chunks of a file makes for an ugly commit history when looking for important changes. This means, don't commit code that doesn't conform to coding standards in the first place. If you do reformat code, make sure it is either standalone reformatting with no logic changes or confined solely to code whose logic you touched. For example, changing the indentation in a file? Do not make logic changes along with it. Editing a line that has extra whitespace at the end? Feel free to remove it.

[complete-issue-list]: //github.com/ponylang/ponyc/search?q=&type=Issues&utf8=%E2%9C%93
[new-issue]: //github.com/ponylang/ponyc/issues/new

Standard Library File Naming
---------------
As of July 2016, the have adopted the following guidelines for naming files in the Pony standard library. New additions to the standard library should conform to this standard:

- The *file name* of Pony source files should be based on the name of the *principal type* defined in that file.
    + The *principal type* in a file is the type that makes up the bulk of the significant lines of code in the file or is conceptually more important or fundamental than all other types in the file. For example, if a file defines a trait type and a group of small class types that all provide that trait, then the trait type should be considered the *principal type*.
    + If there are multiple types defined in the file which all have equal significance and a shared name prefix, then the shared prefix should be used as the *principal type name*. For example, a file that defines `PacketFoo`, `PacketBar`, and `PacketBaz` types should use `Packet` as the *principal type name*, even if no `Packet` type is defined.
    + If there are multiple significant types defined in the file which do not have a shared name prefix, then this should be taken as a hint that these types should probably be defined in separate files instead of together in one file.
- The *file name* should be directly derived from the *principal type name* using a consistent reproducible scheme of case conversion.
    + The *file name* should be the "snake case" version of the *principal type name*. That is, each word in the *principal type name* (as defined by transitions from lowercase to uppercase letters) should be separated with the underscore character (`_`) and lowercased to generate the *file name*. For example, a file that defines the `ContentsLog` type should be named `contents_log.pony`.
    + If the *principal type* is a private type (its name beginning with an underscore character), then the *file name* should also be prefixed with an underscore character to highlight the fact that it defines a private type. For example, a file that defines the `_ClientConnection` type should be named `_client_connection.pony`.
    + If the *principal type* name contains an acronym (a sequence of uppercase letters with no lowercase letters between them), then the entire acronym should be considered as a single word when converting to snake case. Note that if there is another word following the acronym, its first letter will also be uppercase, but should not be considered part of the sequence of uppercase letters that form the acronym. For example, a file that defines the `SSLContext` type should be named `ssl_context.pony`.
