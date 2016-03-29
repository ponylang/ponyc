Contributing
============

It's good to hear that you want to contribute to Pony!

First of all please [search existing issues][complete-issue-list]. Apart from bug reports you will
find feature requests and work currently in progress too. If you cannot find a suitable issue —
[create a new one][new-issue]. Potential pull requests are no exception. Before you start working,
let's have a discussion and agree on the solution and scope.

Each of [bug report](#bug-report), [feature request](#feature-request) or [pull request](#pull-request)
has to cover different aspects described below.

Bug report
----------
Provide the following details:

  - short summary of what you was trying to achieve,
  - a code causing the bug,
  - expected result,
  - actual results and
  - environment details: at least operating system and compiler version (`ponyc -v`).

If possible, try to isolate the problem and provide just enough code to demonstrate it. Add any
related information which might help to fix the issue.

Feature request
---------------
Define a context and a problem the feature solves. List potential use cases and give some code
examples.

Pull request
------------
Provide the same information as you would for a [feature request](#feature-request).

Before issuing a pull request we ask that you squash all your commits into a 
single logical commit. While your PR is in review, we may ask for additional
changes, please do not squash those commits while the review is underway. Once
everything is good, we'll then ask you to further squash those commits before
merging. We ask that you not squash while a review is underway as it can make it
hard to follow what is going on. Additionally, we ask that you:

* [Write a good commit message](http://chris.beams.io/posts/git-commit/)
* Issue 1 Pull Request per feature. Don't lump unrelated changes together.

If you aren't sure how to squash multiple commits into one, Steve Klabnik wrote
[a handy guide](http://blog.steveklabnik.com/posts/2012-11-08-how-to-squash-commits-in-a-github-pull-request)
that you can refer to. 

We keep a [CHANGELOG](CHANGELOG.md) of all software changes with behavioural 
effects in ponyc. If your PR includes such changes (rather than say a
documentation update), please make sure that as part of PR, you have also
updated the CHANGELOG.



[complete-issue-list]: //github.com/ponylang/ponyc/search?q=&type=Issues&utf8=%E2%9C%93
[new-issue]: //github.com/ponylang/ponyc/issues/new
