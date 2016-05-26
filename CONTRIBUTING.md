Contributing
============

It's good to hear that you want to contribute to Pony!

There are a number of ways to contribute to Pony. As this document is a little long, feel free to jump to the section that applies to you currently:

* [Feature request](#feature-request)
* [Bug report](#bug-report)
* [Pull request](#pull-request)

Additional notes regarding formatting:

* [Documentation formatting](#documentation-formatting)
* [Code formatting](#code-formatting)

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

Pull request
------------
Before issuing a pull request we ask that you squash all your commits into a single logical commit. While your PR is in review, we may ask for additional changes, please do not squash those commits while the review is underway. Once everything is good, we'll then ask you to further squash those commits before merging. We ask that you not squash while a review is underway as it can make it hard to follow what is going on. Additionally, we ask that you:

* [Write a good commit message](http://chris.beams.io/posts/git-commit/)
* Issue 1 Pull Request per feature. Don't lump unrelated changes together.

If you aren't sure how to squash multiple commits into one, Steve Klabnik wrote [a handy guide](http://blog.steveklabnik.com/posts/2012-11-08-how-to-squash-commits-in-a-github-pull-request) that you can refer to.

We keep a [CHANGELOG](CHANGELOG.md) of all software changes with behavioural effects in ponyc. If your PR includes such changes (rather than say a documentation update), please make sure that as part of PR, you have also updated the CHANGELOG.

Documentation Formatting
---------------
When contributing to documentation, try to keep the following style guidelines in mind:

* Wherever possible, try to match the style of surrounding documentation.
* Avoid hard-wrapping lines within paragraphs (using line breaks in the middle of or between sentences to make lines shorter than a certain length). Instead, turn on soft-wrapping in your editor and expect the documentation renderer to let the text flow to the width of the container.

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
