primitive _HtmlAssets
  """
  Embedded CSS for the self-contained HTML documentation output.
  """

  fun css(): String =>
    """
    Stylesheet based on the Material-themed stdlib docs layout.
    """
    ":root {\n" +
    "  --bg: #fafafa;\n" +
    "  --fg: #212121;\n" +
    "  --fg-light: #616161;\n" +
    "  --link: #795548;\n" +
    "  --link-hover: #5d4037;\n" +
    "  --border: #e0e0e0;\n" +
    "  --code-bg: #f5f5f5;\n" +
    "  --code-border: #e0e0e0;\n" +
    "  --toc-bg: #fff;\n" +
    "  --header-bg: #795548;\n" +
    "  --header-fg: #fff;\n" +
    "  --nav-bg: #fff;\n" +
    "}\n" +
    "@media (prefers-color-scheme: dark) {\n" +
    "  :root {\n" +
    "    --bg: #1e1e1e;\n" +
    "    --fg: #e0e0e0;\n" +
    "    --fg-light: #9e9e9e;\n" +
    "    --link: #bcaaa4;\n" +
    "    --link-hover: #d7ccc8;\n" +
    "    --border: #424242;\n" +
    "    --code-bg: #2d2d2d;\n" +
    "    --code-border: #424242;\n" +
    "    --toc-bg: #252525;\n" +
    "    --header-bg: #4e342e;\n" +
    "    --header-fg: #e0e0e0;\n" +
    "    --nav-bg: #252525;\n" +
    "  }\n" +
    "}\n" +
    "*, *::before, *::after { box-sizing: border-box; }\n" +
    "body {\n" +
    "  margin: 0;\n" +
    "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI',\n" +
    "    Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;\n" +
    "  font-size: 16px;\n" +
    "  line-height: 1.6;\n" +
    "  color: var(--fg);\n" +
    "  background: var(--bg);\n" +
    "}\n" +
    ".header {\n" +
    "  background: var(--header-bg);\n" +
    "  color: var(--header-fg);\n" +
    "  padding: 0.5rem 1.5rem;\n" +
    "  display: flex;\n" +
    "  align-items: center;\n" +
    "  gap: 0.75rem;\n" +
    "}\n" +
    ".header img {\n" +
    "  height: 32px;\n" +
    "  width: 32px;\n" +
    "}\n" +
    ".header a {\n" +
    "  color: var(--header-fg);\n" +
    "  text-decoration: none;\n" +
    "  font-weight: 600;\n" +
    "  font-size: 1.1rem;\n" +
    "}\n" +
    ".breadcrumb {\n" +
    "  padding: 0.5rem 1.5rem;\n" +
    "  font-size: 0.85rem;\n" +
    "  color: var(--fg-light);\n" +
    "  border-bottom: 1px solid var(--border);\n" +
    "}\n" +
    ".breadcrumb a {\n" +
    "  color: var(--link);\n" +
    "  text-decoration: none;\n" +
    "}\n" +
    ".breadcrumb a:hover { text-decoration: underline; }\n" +
    "main {\n" +
    "  max-width: 52rem;\n" +
    "  margin: 0 auto;\n" +
    "  padding: 1.5rem;\n" +
    "}\n" +
    "a { color: var(--link); }\n" +
    "a:hover { color: var(--link-hover); }\n" +
    "h1 { font-size: 1.8rem; border-bottom: 2px solid var(--border);\n" +
    "  padding-bottom: 0.3rem; margin-top: 0; }\n" +
    "h2 { font-size: 1.4rem; border-bottom: 1px solid var(--border);\n" +
    "  padding-bottom: 0.2rem; margin-top: 2rem; }\n" +
    "h3 { font-size: 1.15rem; margin-top: 1.5rem; }\n" +
    "h4 { font-size: 1rem; margin-top: 1rem; color: var(--fg-light); }\n" +
    "code {\n" +
    "  font-family: 'SFMono-Regular', Consolas, 'Liberation Mono',\n" +
    "    Menlo, monospace;\n" +
    "  font-size: 0.875em;\n" +
    "  background: var(--code-bg);\n" +
    "  padding: 0.15em 0.35em;\n" +
    "  border-radius: 3px;\n" +
    "}\n" +
    "pre {\n" +
    "  background: var(--code-bg);\n" +
    "  border: 1px solid var(--code-border);\n" +
    "  border-radius: 4px;\n" +
    "  padding: 1rem;\n" +
    "  overflow-x: auto;\n" +
    "  line-height: 1.45;\n" +
    "}\n" +
    "pre code {\n" +
    "  background: none;\n" +
    "  padding: 0;\n" +
    "  font-size: 0.85rem;\n" +
    "}\n" +
    ".source-link {\n" +
    "  font-size: 0.85rem;\n" +
    "  margin-left: 0.5rem;\n" +
    "}\n" +
    ".source-link a { text-decoration: none; }\n" +
    ".source-link a:hover { text-decoration: underline; }\n" +
    "hr {\n" +
    "  border: none;\n" +
    "  border-top: 1px solid var(--border);\n" +
    "  margin: 1.5rem 0;\n" +
    "}\n" +
    "ul.type-list {\n" +
    "  list-style: none;\n" +
    "  padding-left: 0;\n" +
    "}\n" +
    "ul.type-list li {\n" +
    "  padding: 0.15rem 0;\n" +
    "}\n" +
    "nav.toc {\n" +
    "  background: var(--toc-bg);\n" +
    "  border: 1px solid var(--border);\n" +
    "  border-radius: 4px;\n" +
    "  padding: 1rem 1.25rem;\n" +
    "  margin: 1.5rem 0;\n" +
    "}\n" +
    "nav.toc h2 {\n" +
    "  font-size: 1.1rem;\n" +
    "  margin: 0 0 0.5rem 0;\n" +
    "  border: none;\n" +
    "  padding: 0;\n" +
    "}\n" +
    "nav.toc ul {\n" +
    "  list-style: none;\n" +
    "  padding-left: 0;\n" +
    "  margin: 0;\n" +
    "}\n" +
    "nav.toc li {\n" +
    "  padding: 0.1rem 0;\n" +
    "  font-size: 0.9rem;\n" +
    "}\n" +
    "nav.toc li a {\n" +
    "  text-decoration: none;\n" +
    "}\n" +
    "nav.toc li a code {\n" +
    "  font-size: 0.8rem;\n" +
    "}\n" +
    "nav.toc li a:hover { text-decoration: underline; }\n" +
    "nav.toc h3 {\n" +
    "  font-size: 0.95rem;\n" +
    "  margin: 0.75rem 0 0.25rem 0;\n" +
    "  border: none;\n" +
    "  color: var(--fg-light);\n" +
    "}\n" +
    "nav.toc h3:first-child { margin-top: 0; }\n" +
    ".params li, .returns li {\n" +
    "  padding: 0.1rem 0;\n" +
    "}\n" +
    ".source-table {\n" +
    "  width: 100%;\n" +
    "  border-collapse: collapse;\n" +
    "  background: var(--code-bg);\n" +
    "  border: 1px solid var(--code-border);\n" +
    "  border-radius: 4px;\n" +
    "  padding: 0.5rem;\n" +
    "  overflow-x: auto;\n" +
    "  display: block;\n" +
    "}\n" +
    ".source-table td {\n" +
    "  padding: 0;\n" +
    "  vertical-align: top;\n" +
    "  font-family: 'SFMono-Regular', Consolas, 'Liberation Mono',\n" +
    "    Menlo, monospace;\n" +
    "  font-size: 0.85rem;\n" +
    "  line-height: 1.45;\n" +
    "}\n" +
    ".source-table .line-num {\n" +
    "  width: 1%;\n" +
    "  white-space: nowrap;\n" +
    "  padding-right: 1rem;\n" +
    "  text-align: right;\n" +
    "  color: var(--fg-light);\n" +
    "  user-select: none;\n" +
    "  -webkit-user-select: none;\n" +
    "}\n" +
    ".source-table .line-num a {\n" +
    "  color: var(--fg-light);\n" +
    "  text-decoration: none;\n" +
    "}\n" +
    ".source-table .line-num a:hover {\n" +
    "  color: var(--link);\n" +
    "}\n" +
    ".source-code { white-space: pre; }\n"
