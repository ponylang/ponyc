# Instructions for modifying lib/

FIXME: `git subtree --fetch` shoud not default to getting tags
because when you fetch tags they'll replace any existing tags
of the same name. I assume this is because that is the default
behavior of `git fetch`. Why can't tags have a name space like
branches do?

This directory contains third-party libraries and
in order to make updating as easy as possible changes
to the contents of any of the subdirectories should
be done with forethought.

A goal for managing the libraries is to not change
nominal workflows. In particular, checking out this
repo or committing, pushing, fetching, pulling and
merging changes excetera; will not require any
alteration to the workflow.

On the other hand, changes to the contents of this
directory will require changes in workflow.

There are currently two methods for maintaining the
libraries. The preferred method is using `git subtree`
and the alternative is using patches. For details see
[Git `subtree` managed libraries](#git-subtree-managed-libraries)
and [Patch managed libraries](#patch-managed-libraries).

## Libraries managed using `git subtree`:

- gbenchmark

## Libraries managed using patches:

- blake2
- gtest

## Management techniques

### Git `subtree` managed libraries:

#### Add a new library as a subtree

We'll assume a new library you'd like to use is at
`github.com/google/benchmark.git` and the directory
we'll store it under is `gbenchmark`.

1. Choose a name, such as `google-benchmark`, for tracking
the remote repo `benchmark.git` and fetch it `(-f)` without tags (--no-tags):
```
$ git remote add -f --no-tags google-benchmark git@github.com:google/benchmark
Updating google-benchmark
From github.com:google/benchmark
 * [new branch]        clangtidy       -> google-benchmark/clangtidy
 * [new branch]        master          -> google-benchmark/master
 * [new branch]        releasing       -> google-benchmark/releasing
 * [new branch]        reportercleanup -> google-benchmark/reportercleanup
 * [new branch]        rmheaders       -> google-benchmark/rmheaders
 * [new branch]        v2              -> google-benchmark/v2
```
2. Decide what tags or sha1's you like to have access to. You can use
[Display tags](display-tags) and [Copy tags](copy-tags) if
you'd like. For instance to display and copy the tags for
google-benchmark:
```
$ tools/dsptags.sh url=google-benchmark
v0.0.9 (at a5c8da0c9a598cdfb3fc58aa15e88bd966b671eb)
v0.1.0 (at 73b734eb1fe857296e49528cd2413640aab7c40e)
v1.0.0 (at 10b5a3ed476ead9d41f4ae698c3cf3dec42cb0b7)
v1.1.0 (at 7ba0cafef7db090a21fefcdfafe3448dcfe8a241)
v1.2.0 (at 163ce4af68fd7d100e7e54ab65abd50971183009)
v1.3.0 (at 36c7e98550ed12e2a11a11e22a6151aa58b4954a)
v1.4.0 (at 54d92f930f91d640c1300d3dadee35fb73c1933b)

$ tools/cpytags.sh url=google-benchmark regex=v1.* prefix=gb-
Make tag 'gb-v1.0.0' (at 10b5a3ed476ead9d41f4ae698c3cf3dec42cb0b7)
Make tag 'gb-v1.1.0' (at 7ba0cafef7db090a21fefcdfafe3448dcfe8a241)
Make tag 'gb-v1.2.0' (at 163ce4af68fd7d100e7e54ab65abd50971183009)
Make tag 'gb-v1.3.0' (at 36c7e98550ed12e2a11a11e22a6151aa58b4954a)
Make tag 'gb-v1.4.0' (at 54d92f930f91d640c1300d3dadee35fb73c1933b)
```

3. Choose a commit you want to add as a `subtree`, gb-v1.4.0, and merge
into pony at `lib/gbenchmark`:
```
git subtree add --prefix lib/gbenchmark --squash google-benchmark gb-v1.4.0
```
`lib/gbenchmark` is now created and v1.4.0 merged into pony's repo.

#### Create a `subtree` managed library from an existing library.

This is actually the same as creating a new library execpt preceeded
by deleting the current directory and committing it. We'll assume
`lib/gbenchmark` was previously existed.

1. Delete `lib/gbenchmark`:
```
$ rm -rf lib/gbenchmark
```
2. Commit, as you cannot use `subtree add` except on a clean working tree:
```
$ git commit -a -m "Delete lib/gbenchmark in preparation for adding as subtree"
```
Now proceed with the steps in [Add a new library as a subtree](#add-a-new-library-as-a-subtree).

#### Rebasing across subtree merges that are simple

By simple, you need to rebase a subtree you're working on to master that has been updated.

We start with:
```
$ git log --graph  --pretty="format:%h %s" master..
* df6b88f9 Updates to use gbenchmark plus some tools and documentation.
*   26eca0c8 Subtree add google/benchmark a9beffda0b89a699 as lib/gbenchmark
|\
| * 013d660a Squashed 'lib/gbenchmark/' content from commit a9beffda
* 1c4cd1b0 Delete lib/gbenchmark in preparation for updating to newer version.
```
Do a rebase but you must use `--preserve-merges` if the change is trival it will succeed and you're done. If you get the `fatal: refusing ..` error such as below:
```
$ git rebase --preserve-merges master
fatal: refusing to merge unrelated histories
Error redoing merge 26eca0c8ec80011cb400416207cf15df46e13f54
```
This is a bug in git and you need to re-execute the the `subtree add`:
```
$ git subtree add --prefix lib/gbenchmark --squash google-benchmark master
git fetch google-benchmark master
remote: Counting objects: 48, done.
remote: Compressing objects: 100% (2/2), done.
remote: Total 48 (delta 30), reused 32 (delta 30), pack-reused 16
Unpacking objects: 100% (48/48), done.
From github.com:google/benchmark
 * branch              master     -> FETCH_HEAD
   a9beffda..e7eb54b5  master     -> google-benchmark/master
Added dir 'lib/gbenchmark'
```
And then contine the rebase:
```
$ git rebase --continue
Successfully rebased and updated refs/heads/Use-git-subtree-to-manage-third-party-libraries.
```

#### Rebasing after subtree merge, a non-trivial case.

The non-trivial case is to move `Add x.txt` to before `Delete lib/gbenchmark`. This is similar to the trival case above but we'll manually use `edit` to pause the rebase so we can redo the `subtree add`.

We start with:
```
$ git log --graph  --pretty="format:%h %s" master..
* 4f61a145 Add x.txt
* df6b88f9 Updates to use gbenchmark plus some tools and documentation.
*   26eca0c8 Subtree add google/benchmark a9beffda0b89a699 as lib/gbenchmark
|\
| * 013d660a Squashed 'lib/gbenchmark/' content from commit a9beffda
* 1c4cd1b0 Delete lib/gbenchmark in preparation for updating to newer version.
```
Now do rebase interactive with preserve-merges:
```
$ git rebase -i --preserve-merges master
```
And the editor says:
```
pick 1c4cd1b0 Delete lib/gbenchmark in preparation for updating to newer version.
pick 013d660a Squashed 'lib/gbenchmark/' content from commit a9beffda
pick df6b88f9 Updates to use gbenchmark plus some tools and documentation.
pick 4f61a145 Add x.txt
```
Notice the missing `Subtree add google/benchmark`, as before this is a bug in git.
What you do now is delete `Squashed 'lib/gbenchmark/'...` and mark the
`delete `lib/gbenchmark` with `edit` so the rebase will pause and you can
reapply the `subtree add`:
```
pick 4f61a145 Add x.txt
edit 1c4cd1b0 Delete lib/gbenchmark in preparation for updating to newer version.
pick df6b88f9 Updates to use gbenchmark plus some tools and documentation.
```
Now write/quit editor and we are requested to --amend and then --continue
```
$ git rebase -i master
Stopped at 1c4cd1b0...  Delete lib/gbenchmark in preparation for updating to newer version.
You can amend the commit now, with

  git commit --amend

Once you are satisfied with your changes, run

  git rebase --continue
```
We're not going to change anything in `1c4cd1b0`, we're just pausing to reapply
`subtree add`, so we just do `git commit --amend` and write/quit the editor:
```
$ git commit --amend
[detached HEAD 611cf5ad] Delete lib/gbenchmark in preparation for updating to newer version.
 Date: Sat Mar 10 10:56:19 2018 -0800
 70 files changed, 12130 deletions(-)
 delete mode 100644 lib/gbenchmark/.clang-format
...
 delete mode 100644 lib/gbenchmark/tools/gbench/util.py
```
Now redo the subtree add, as it was originally done:
```
$ git subtree add --prefix lib/gbenchmark --squash google-benchmark master
git fetch google-benchmark master
From github.com:google/benchmark
 * branch              master     -> FETCH_HEAD
Added dir 'lib/gbenchmark'
```
And finally, continue the rebase:
```
$ git rebase --continue
Successfully rebased and updated refs/heads/test-x.
```
All is now well, we have reordered the commits with `Add x.txt` before the `Delete lib/gbenchmark`
and `subtree add` commits are present:
```
$ git log --graph  --pretty="format:%h %s" master..test-x
* 61e14ba3 Updates to use gbenchmark plus some tools and documentation.
*   ee002130 Merge commit 'b5f2f6aeaaabcbcae1605607101904add62a73eb' as 'lib/gbenchmark'
|\
| * b5f2f6ae Squashed 'lib/gbenchmark/' content from commit a9beffda
* 01853e6e Delete lib/gbenchmark in preparation for updating to newer version.
* 7144c2a8 Add x.txt
```

### Patch managed libraries:

TODO

## Tools

### Display tags

Display tags from a repo either via a url or the local repo with an
optional regex, help:
```
$ ./tools/dsptags.sh help
Display tags from current or remote repo
  Usage: ./tools/dsptags.sh {url=xxx} {regex=yyy}
    url=xxx   xxx is the url of the remote repo or empty local
    regex=yyy yyy is the regular expression to match tags
              regex empty, yyy=* or yyy=all for all tags
    help      Displays this help message

  A parameter in braces is optional
```

Display tags that begin with 'v' from google/benchmark.git on github:
```
$ ./tools/dsptags.sh url=git@github.com:google/benchmark.git regex=v.*
v0.0.9 (at a5c8da0c9a598cdfb3fc58aa15e88bd966b671eb)
v0.1.0 (at 73b734eb1fe857296e49528cd2413640aab7c40e)
v1.0.0 (at 10b5a3ed476ead9d41f4ae698c3cf3dec42cb0b7)
v1.1.0 (at 7ba0cafef7db090a21fefcdfafe3448dcfe8a241)
v1.2.0 (at 163ce4af68fd7d100e7e54ab65abd50971183009)
v1.3.0 (at 336bb8db986cc52cdf0cefa0a7378b9567d1afee)
```

If there a remote has been setup then the url can be the remote:
```
$ git remote -v
origin	git@github.com:winksaville/ponyc (fetch)
origin	git@github.com:winksaville/ponyc (push)
google-benchmark	git@github.com:google/.git (fetch)
google-benchmark	git@github.com:google/benchmark.git (push)
upstream	git@github.com:ponylang/ponyc.git (fetch)
upstream	git@github.com:ponylang/ponyc.git (push
$ ./tools/dsptags.sh url=google-benchmark regex=v1.*
v1.0.0 (at 10b5a3ed476ead9d41f4ae698c3cf3dec42cb0b7)
v1.1.0 (at 7ba0cafef7db090a21fefcdfafe3448dcfe8a241)
v1.2.0 (at 163ce4af68fd7d100e7e54ab65abd50971183009)
v1.3.0 (at 336bb8db986cc52cdf0cefa0a7378b9567d1afee)
```

If we'd copied some tags, see [below](#copy-tags-cpytags.sh), with
prefix 'gb-', we can display them with:
```
$ ./tools/dsptags.sh regex=gb-.*
gb-v1.0.0 (at 10b5a3ed476ead9d41f4ae698c3cf3dec42cb0b7)
gb-v1.1.0 (at 7ba0cafef7db090a21fefcdfafe3448dcfe8a241)
gb-v1.2.0 (at 163ce4af68fd7d100e7e54ab65abd50971183009)
gb-v1.3.0 (at 336bb8db986cc52cdf0cefa0a7378b9567d1afee)
```

### Copy tags

Copy tags from a repo via a url to the local repo with
a prefix and and optional regex (Note, the prefix is not
optional so as to minimize naming collisions):
```
$ ./tools/cpytags.sh help
Copy tags from remote repo to the local repo
  Usage: ./tools/cpytags.sh url=xxx prefix=yyy {regex=zzz}
    url=xxx    xxx is the url of the remote repo
    prefix=yyy yyy is the prefix to add to the tag
    regex=yyy  yyy is the regular expression to match tags
               regex empty, yyy=* or yyy=all for all tags
    help       Displays this help message

  A parameter in braces is optional
```

Copy some tags from google/benchmark and prefix them with 'gb-':
```
$ ./tools/cpytags.sh url=git@github.com:google/benchmark.git prefix=gb- regex=v1.*
Make tag 'gb-v1.0.0' (at 10b5a3ed476ead9d41f4ae698c3cf3dec42cb0b7)
Make tag 'gb-v1.1.0' (at 7ba0cafef7db090a21fefcdfafe3448dcfe8a241)
Make tag 'gb-v1.2.0' (at 163ce4af68fd7d100e7e54ab65abd50971183009)
Make tag 'gb-v1.3.0' (at 336bb8db986cc52cdf0cefa0a7378b9567d1afee)
```

### Delete local tags

Delete tags from local repo `deltags.sh`
```
$ ./tools/deltags.sh help
Delete tags from local  repo
  Usage: ./tools/deltags.sh regex=yyy
    regex=yyy yyy is the regular expression to match tags
              yyy=* or yyy=all for all tags
    help      Displays this help message
```

Delete tags that begin with 'gb' from local repo:
```
$ ./tools/deltags.sh regex='gb*'
gb-v1.0.0 gb-v1.1.0 gb-v1.2.0 gb-v1.3.0
Delete? (y/N): y
Deleted tag 'gb-v1.0.0' (was 10b5a3ed)
Deleted tag 'gb-v1.1.0' (was 7ba0cafe)
Deleted tag 'gb-v1.2.0' (was 163ce4af)
Deleted tag 'gb-v1.3.0' (was 336bb8db)
```
