## Fix pony-lint blank-lines rule false positives on multi-line docstrings

The `style/blank-lines` rule incorrectly counted blank lines inside multi-line docstrings as blank lines between members. A method or field whose docstring contained blank lines (e.g., between paragraphs) would be flagged for having too many blank lines before the next member. The rule now correctly identifies where a docstring ends rather than using only its start line.
