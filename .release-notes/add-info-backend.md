## Add GNU Info backend to pony-doc

pony-doc can now generate GNU Info documentation with `--backend info`. The output is a single `.info` file readable by any Info viewer — Emacs, standalone `info`, or `pinfo`.

```bash
pony-doc --backend info ./my-package
```

The default backend remains `mkdocs`, so existing invocations are unchanged.
