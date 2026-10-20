## Add GNU Info backend to pony-doc

pony-doc can now generate GNU Info documentation with `--backend info`. The output is a single `.info` file readable by any Info viewer — Emacs, standalone `info`, or `pinfo`.

```bash
pony-doc --backend info ./my-package
```

The default backend remains `mkdocs`, so existing invocations are unchanged.

## Fix Base64.encode_url emitting NUL bytes instead of stripping padding

`Base64.encode_url` with the default `pad = false` wrote NUL bytes where padding characters would go, instead of omitting them. The NUL-padded output was then rejected by `Base64.decode_url` as invalid base64.

`Base64.encode_url` now strips padding entirely when `pad = false`.

