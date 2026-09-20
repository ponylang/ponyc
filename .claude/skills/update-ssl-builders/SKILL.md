---
name: update-ssl-builders
description: Twice-yearly workflow for bumping ponyc CI SSL builder images (OpenSSL, LibreSSL) to their latest series releases. Invoked via /update-ssl-builders.
disable-model-invocation: false
---

# Update SSL Builders

ponyc CI tests `packages/net/` and `packages/crypto/` against multiple SSL implementations. Each has its own Docker builder image under `.ci-dockerfiles/ubuntu26.04-builder-with-<library>-<version>/`. This skill walks through bumping those builders to the latest upstream releases.

## Background

**One version per series**: These are internal CI builders. Each series keeps exactly one version — an update replaces the old version with the new one in a single PR.

**OpenSSL 1.1.1 is EOL**: The `openssl-1.1.1w` builder exists for backward-compatibility testing. This skill does not update or drop it.

**Builders are amd64-only**: Each builder directory contains two files: `Dockerfile` and `build-and-push.bash`. No multiplatform build, no `combine-images.bash`, no `README.md`.

**SSL flag mapping**: The `ssl-backends-test` matrix in `pr.yml` maps each SSL library to a Pony compiler flag:

| Library series | Pony flag |
|---|---|
| `libressl-*` | `-Dlibressl` |
| `openssl-3.*` | `-Dopenssl_3.0.x` |
| `openssl-4.*` | `-Dopenssl_4.0.x` |

If a new major version line appears upstream (e.g., OpenSSL 5.x) that has no existing Pony compiler flag, do not add a builder. Open a task issue on this repo to add support for testing with the new version, noting the upstream release.

**Three workflow files reference SSL builders**:

- `rebuild-ssl-builder-images.yml` — `matrix.ssl` list; rebuilds all builders when the base image changes.
- `build-builder-image.yml` — `inputs.builder-name` dropdown and per-builder job blocks; builds a single chosen builder on demand.
- `pr.yml` — `ssl-backends-test.strategy.matrix.include`; runs `net` and `crypto` tests against each builder image.

## Phase 1: Inventory

List all `ubuntu26.04-builder-with-*` directories under `.ci-dockerfiles/` and group them by library + series. Ignore `openssl-1.1.1w`.

## Phase 2: Fetch upstream releases

- LibreSSL: https://www.libressl.org/releases.html
- OpenSSL: https://openssl-library.org/source/

For each library, find the highest version number available across all currently-supported upstream branches.

## Phase 3: Compute updates and confirm

**Series definition (deliberate project policy)**: One major version line = one series. LibreSSL 3.x, LibreSSL 4.x, OpenSSL 3.x, OpenSSL 4.x are each a single series. This collapses OpenSSL's concurrently-supported LTS branches (e.g., 3.0/3.4/3.5/3.6) into one series — a new release in any 3.x branch counts as "the latest of OpenSSL 3.x."

For each series (excluding OpenSSL 1.x):
- If the upstream latest is newer than what we have, plan to **replace** the current version with the new one.
- If upstream is not newer, no changes for that series.

Present the plan to the user and wait for explicit confirmation before making any changes.

## Phase 4: Per-update workflow (one PR per series)

Each series update gets its own PR. The PR replaces the old builder with the new one.

1. Create a new branch.
2. Create a new directory `.ci-dockerfiles/ubuntu26.04-builder-with-<library>-<new-version>/`.
3. Copy the two files from the current version in the same series: `Dockerfile` and `build-and-push.bash`.
4. Bump version references in each file:
   - **Dockerfile**: The version ARG (`OPENSSL_VERSION` or `LIBRESSL_VERSION`). The download URL pattern is stable within a major version line and should not need changing.
   - **build-and-push.bash**: The `NAME` variable (full GHCR image path including the version) and the `BUILDER` variable (strip dots and dashes from the version, e.g., `ssl-openssl372`, `ssl-libressl430`).
5. Delete the old directory `.ci-dockerfiles/ubuntu26.04-builder-with-<library>-<old-version>/`.
6. Edit `.github/workflows/rebuild-ssl-builder-images.yml`: replace the old `<library>-<old-version>` entry in `matrix.ssl` with `<library>-<new-version>`.
7. Edit `.github/workflows/build-builder-image.yml`:
   - Replace the old builder name with the new one in the `options` list.
   - Replace the old job block with a new one. The job key uses underscores for dots (e.g., `ubuntu26_04-builder-with-openssl-3_7_0`). Copy the structure from the old job, updating the `if:` condition, `name:` field, and `run:` step to reference the new directory.
8. Commit, push, open PR. Do **not** update `pr.yml` yet — there is no image to reference.
9. Dispatch the build workflow on the branch to build and push the new image to GHCR:
   ```
   gh workflow run build-builder-image.yml --ref <branch> -f ref=<branch> -f builder-name=ubuntu26.04-builder-with-<library>-<new-version>
   ```
   `--ref <branch>` is required — the new builder name only exists in the branch's `choice` input list. The image is tagged with today's date (`YYYYMMDD` from `build-and-push.bash`).
10. Wait for the workflow run to complete successfully. Verify the image exists on GHCR:
    ```
    gh api orgs/ponylang/packages/container/ponyc-ci-ubuntu26.04-builder-with-<library>-<new-version>/versions --jq '.[0].metadata.container.tags'
    ```
11. Edit `.github/workflows/pr.yml`: in `ssl-backends-test.strategy.matrix.include`, replace the old entry with the new one:
    ```yaml
    - ssl-name: <library>-<new-version>
      ssl-flag: '<flag from mapping table>'
      image: ghcr.io/ponylang/ponyc-ci-ubuntu26.04-builder-with-<library>-<new-version>:<YYYYMMDD>
    ```
12. Commit and push.
13. Wait for PR CI to pass. The `ssl-backends-test` suite should now include the new builder and its `net`/`crypto` tests should pass.
14. Squash-merge the PR.

Existing tagged images for the old version on GHCR are left intact — they simply stop receiving updates because they are no longer in the rebuild workflow's matrix.
