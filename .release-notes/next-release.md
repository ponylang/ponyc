## "Exclude source files" from documentation source

In the previous ponyc release, we updated the default theme used by our documentation generation from a custom theme based on mkdocs-material to using the latest mkdocs-material. With the change came the possibility of using all the latests mkdocs-material features. [Mkdocs-material](https://squidfunk.github.io/mkdocs-material/) is a heavily developed theme that has a ton of great features. Some of mkdocs-material's features are only available to [sponsors of the project](https://squidfunk.github.io/mkdocs-material/insiders/).

One of the many excellent insider's features is the ability to exclude some files from the mkdocs search index. We've added support for this feature in the generated documentation so that, **iff** you are a mkdocs-material sponsor, the source code files that we link to from generated documentation will no longer be included in the source index.

If you've used our generated documentation extensively, you know that having the source implementation linked is a great boon, but it is also a pain as it clutters up the search index and you often end up on a source implementation page from a search.

If you are using the built in documentation generation that comes with ponyc, we suggest that you take a look into becoming a sponsor of mkdocs-material as we will be adding support for additional insiders features if folks find them to be useful as part of the generated documentation.

