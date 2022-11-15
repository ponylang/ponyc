## Don't include generated documentation "home file" in search index

We've updated the documentation generation to not include the "home file" that lists the packages in the search index. The "exclude from search engine" functionality is only available in the `mkdocs-material-insiders` theme.

The home file is extra noise in the index that provides no value. For anyone using the insiders theme, this will be an improvement in the search experience.

