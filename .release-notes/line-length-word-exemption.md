## pony-lint line-length rule narrows unbreakable-word exemption

The `style/line-length` rule now exempts a line only when one of the first two space-delimited words crosses column 80. A long word that appears later on the line — after other content — is flagged, because the content before it can go on a separate line.

This replaces the previous string-literal exemption. Lines like `// https://very-long-url` are exempt (the URL is the second word and cannot be shortened by breaking the line). Lines like `// some text https://very-long-url` are flagged — "some text" and the URL can go on separate comment lines, and the URL-only line is then exempt on its own.
