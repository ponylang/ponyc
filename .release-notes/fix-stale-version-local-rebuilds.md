## Fix stale version and git hash in local rebuilds

Rebuilding ponyc from the same checkout after pulling new commits showed the old version string and git hash until cmake was re-run from scratch. Installing that build wrote the stale version into the install, so `ponyc --version` reported the wrong version even after upgrading. CI builds were unaffected because they always configure fresh.
