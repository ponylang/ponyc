## Fix pony-lint FileNaming false positives on Windows

pony-lint's FileNaming rule produced false positives on Windows because the filename extraction logic only recognized `/` as a path separator. On Windows, where paths use `\`, the full path including directory components was compared against the expected type name, causing every file to be flagged.
