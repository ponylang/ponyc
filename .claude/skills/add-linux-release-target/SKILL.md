---
name: add-linux-release-target
description: Load when adding a new Linux distro/version (e.g. Ubuntu 26.04, Alpine 3.24) as a ponyc release target. Covers builder Dockerfile creation, image build dispatch and tag discovery, workflow updates, docs, release notes, and the post-merge Last Week in Pony announcement.
disable-model-invocation: false
---

# Adding a New Linux Distro/Version as a ponyc Release Target

## Scope

Use this skill when adding a new distro/version (e.g. Ubuntu 26.04) so ponyc produces release and nightly artifacts users can install via ponyup.

The procedure encodes the **multi-platform builder pattern** — a single `.ci-dockerfiles/<distro><version>-builder/` directory containing one Dockerfile and one `build-and-push.bash` that builds and publishes a multi-arch image (linux/amd64 + linux/arm64) under `ghcr.io/ponylang/ponyc-ci-<distro><version>-builder`. This is the pattern used by Alpine 3.22+. New release-target adds — including new Ubuntu versions — should use it. Older builders still use a per-arch pattern (separate `x86-64-unknown-linux-...-builder/` and `arm64-unknown-linux-...-builder/` directories); leave those alone, don't migrate them as part of an add.

**Out of scope:**
- CI test-only workflows: `pr-ponyc.yml`, `ponyc-tier2.yml`, `ponyc-weekly-checks.yml`, `stress-test-*.yml`. These reference Linux images for testing ponyc itself, not for producing release artifacts. Don't touch them.
- `pr-docker-image-validation.yml` validates the user-facing `.dockerfiles/{nightly,release}/Dockerfile` images (Alpine-based). Not affected by adding a release target.
- Removing EOL'd distros. Different procedure, not covered here.
- Migrating an existing per-arch builder to the multi-platform pattern. Different procedure, not covered here.

## Guiding facts

- **Purely additive.** Older supported versions stay until their own upstream EOL. Don't propose dropping anything as part of this work.
- **Both architectures by default.** Multi-platform images build linux/amd64 and linux/arm64 from one Dockerfile. Don't restrict arches without a concrete reason.
- **GHCR builder packages are repo-scoped.** No manual "make public" step is needed for new packages — same-repo CI pulls them automatically.
- **Mental model.** One multi-arch builder image gets pushed to GHCR with a date-stamp tag. Workflow files (`release.yml`, `nightlies.yml`, `update-lib-cache.yml`) reference that one image by exact tag from both their x86_64-linux and arm64-linux matrices — Docker pulls the matching arch on each runner. Adding a new release target = create the Dockerfile, build/push the image, then thread the resulting tag into every workflow that consumes it, plus update the user-facing docs.

## File inventory

New files (a single builder directory with two files):
- `.ci-dockerfiles/<distro><version>-builder/Dockerfile`
- `.ci-dockerfiles/<distro><version>-builder/build-and-push.bash`

Existing files to edit:
- `.github/workflows/build-builder-image.yml` — one dropdown option + one job block
- `.github/workflows/release.yml` — `x86_64-linux` and `arm64-linux` matrices
- `.github/workflows/nightlies.yml` — same matrices
- `.github/workflows/update-lib-cache.yml` — same matrices (simpler shape: just `image:`)
- `INSTALL.md` — supported-distro list + PLATFORM string table
- `RELEASE_PROCESS.md` — package names list

New release notes file:
- `.release-notes/add-<distro>-<version>.md`

`BUILD.md` and `README.md` need no changes — neither contains version-specific content for this kind of add.

## Before you begin

- **Branch base:** start from a fresh `main`. `git checkout main && git pull`.
- **Tools:** Docker daemon running (`docker info` succeeds), `gh` authenticated, `git` configured to push to origin.
- **`gh` token scopes needed:** `workflow` (for dispatching builds) and `read:packages` (for querying GHCR tags). Check with `gh auth status`. If missing: `gh auth refresh -s workflow,read:packages`.
- **Placeholders in this document:** every `<distro>`, `<version>`, `<tag>`, etc. is a placeholder for substitution. The angle brackets are not part of any command. Example for Ubuntu 26.04: `<distro>` → `ubuntu`, `<version>` → `26.04`.

## Procedure

The **structural exemplar** — directory layout, `build-and-push.bash`, `build-builder-image.yml` job block, workflow matrix entries — is the most recent Alpine multi-platform builder. As of writing, that's `alpine3.23-builder/`. Before starting, glance at `.ci-dockerfiles/` and pick whichever Alpine multi-platform builder dir is newest (`alpineX.Y-builder/`, no arch prefix); use it in place of `alpine3.23-builder` throughout this document.

The **Dockerfile-content exemplar** is the most recent same-distro builder, regardless of pattern (per-arch is fine — only the package list and base image are reused). For Ubuntu adds at the time this skill was written, that's `x86-64-unknown-linux-ubuntu24.04-builder/Dockerfile`.

### 1. Branch

```bash
git checkout main && git pull
git checkout -b add-<distro>-<version>-release-target
```

### 2. Create the builder Dockerfile and build-and-push script

```bash
mkdir -p .ci-dockerfiles/<distro><version>-builder
```

**Dockerfile.** Copy the body from the most recent same-distro builder's Dockerfile (for Ubuntu, the per-arch x86-64 and arm64 Ubuntu 24.04 Dockerfiles are byte-identical, so either is fine), change the `FROM` line to the new version. The base image (e.g. `ubuntu:26.04`) must be a multi-arch manifest — it already is for the official `ubuntu` and `alpine` images. If adding a different distro, verify with:

```bash
docker manifest inspect <distro>:<version> | jq '.manifests[].platform'
```

Both `linux/amd64` and `linux/arm64` must appear.

**build-and-push.bash.** Copy from the structural-exemplar multi-platform builder (e.g. `alpine3.23-builder/build-and-push.bash`) and change only the `NAME` and the `BUILDER` prefix. The shape:

```bash
#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

NAME="ghcr.io/ponylang/ponyc-ci-<distro><version>-builder"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"
BUILDER="<distro><version>-builder-$(date +%s)"

docker buildx create --use --name "${BUILDER}"
docker buildx build --provenance false --sbom false --platform linux/arm64,linux/amd64 --pull --push -t "${NAME}:${TODAY}" "${DOCKERFILE_DIR}"
docker buildx rm "${BUILDER}"
```

Don't modify the package list, the `LABEL`, the user setup, or anything else without a concrete reason. If a package name has changed in the new distro version, the local smoke test in step 3 will surface it — don't pre-emptively edit.

### 3. Local smoke test

Before pushing or dispatching anything, build the Dockerfile locally for amd64 explicitly:

```bash
docker build --pull --platform linux/amd64 .ci-dockerfiles/<distro><version>-builder/
```

`--platform linux/amd64` makes the smoke test deterministic regardless of host architecture (matters on Apple Silicon, where host-default would be `linux/arm64` and use a different package set). `--pull` forces Docker to fetch the latest base image rather than use a stale local cache. This is a smoke test only — don't run `build-and-push.bash` locally (that would push to GHCR with today's date stamp).

If the build fails because Docker isn't running or the package manager can't reach the network, that's an environment problem — fix it and retry, don't escalate. If the build fails inside the image (package gone, repo URL invalid for the new distro, base image not yet published, etc.): **STOP**. Surface the error to the human. Don't retry. Don't "fix" by tweaking the package list without confirmation — that's a design discussion.

The remote multi-platform build in step 6 cross-builds arm64 from x86-64 via QEMU. Local x86-64 success is a strong signal but not a guarantee for arm64; the GHA dispatch is the real arm64 verification.

### 4. Update `build-builder-image.yml`

Two edits, both mechanical:

**a. Add one dropdown option** (alphabetical across the whole list):

```yaml
options:
  ...
  - <distro><version>-builder
  ...
```

**b. Add one job block** by copying the structural-exemplar job (e.g. `alpine3_23-builder`) and substituting the version. Place the new job alphabetically among the sibling jobs.

The job map key replaces `.` with `_` (YAML map keys can't contain `.`): a version of `26.04` becomes `26_04` in the job key only — the `if:` filter value, the `name:` field, and the `bash` script path all keep the dot form. Reference shape:

```yaml
<distro><version_underscore>-builder:
  if: ${{ github.event.inputs.builder-name == '<distro><version>-builder' }}
  runs-on: ubuntu-latest

  name: <distro><version>-builder
  steps:
    - name: Checkout
      uses: actions/checkout@v6.0.2
    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v4
      with:
        version: v0.23.0
    - name: Login to GitHub Container Registry
      uses: docker/login-action@v4
      with:
        registry: ghcr.io
        username: ${{ github.repository_owner }}
        password: ${{ secrets.GITHUB_TOKEN }}
    - name: Build and push
      run: bash .ci-dockerfiles/<distro><version>-builder/build-and-push.bash
```

`runs-on: ubuntu-latest` is correct — the multi-platform build cross-compiles arm64 via QEMU on an x86-64 runner. Don't change it.

### 5. Push branch (WIP commit is fine)

```bash
git add .ci-dockerfiles .github/workflows/build-builder-image.yml
git commit -m "WIP: add <distro> <version> builder"
git push -u origin add-<distro>-<version>-release-target
```

### 6. Dispatch the builder workflow from the branch

GHA `workflow_dispatch` runs the workflow file from the chosen ref, so the new dropdown option becomes available when targeting the branch.

```bash
gh workflow run build-builder-image.yml \
  --ref add-<distro>-<version>-release-target \
  -f builder-name=<distro><version>-builder
```

If dispatch returns 403, the `gh` token lacks `workflow` scope — see "Before you begin."

### 7. Wait for the build to finish

The multi-platform build takes ~20–40 minutes (arm64 via QEMU is the slow leg). Capture the run id, then watch it:

```bash
RUN_ID=$(gh run list --workflow=build-builder-image.yml \
  --branch=add-<distro>-<version>-release-target \
  --limit=1 --json databaseId --jq '.[].databaseId')

gh run watch "$RUN_ID" --exit-status
```

If the build fails: **STOP**. Read the failed logs with `gh run view "$RUN_ID" --log-failed` and surface the failure to the human. A common cause is a package or repo change for the new distro — that's a discussion, not something to silently work around.

### 8. Discover the published tag

The successful build pushes `ghcr.io/ponylang/ponyc-ci-<distro><version>-builder:YYYYMMDD`. Query GHCR:

```bash
gh api -X GET /orgs/ponylang/packages/container/ponyc-ci-<distro><version>-builder/versions \
  --jq '[.[].metadata.container.tags[]] | map(select(test("^[0-9]{8}$"))) | max'
```

The `select(test("^[0-9]{8}$"))` filter restricts to date-form tags (rejects `latest` or other future tags); `max` picks the most recent.

If the API returns 403, the `gh` token lacks `read:packages` — see "Before you begin."

### 9. Apply the tag to workflow files

Add new matrix entries alongside the existing entries. The same image string goes into both the `x86_64-linux` and `arm64-linux` matrices — Docker pulls the matching arch on each runner.

**Use the structural-exemplar entry (Alpine 3.23) from the same file/matrix as your template.** Existing same-distro entries are inconsistent (per-arch builders use different image-name shapes; some files have `triple-vendor: unknown` on arm64, others omit it). Avoid that ambiguity: copy the alpine3.23 entry from the file/matrix you're editing, then substitute `image:`, `name:`, and `triple-os:` with the new distro/version values. Keep all other fields (including `triple-vendor`'s presence or absence) exactly as alpine3.23 has them in that location.

Files and matrices to edit:
- `.github/workflows/release.yml` — `x86_64-linux` and `arm64-linux` matrices.
- `.github/workflows/nightlies.yml` — `x86_64-linux` and `arm64-linux` matrices.
- `.github/workflows/update-lib-cache.yml` — `x86_64-linux` and `arm64-linux` matrices (simpler shape: just `image:`).

Worked example for Ubuntu 26.04, x86_64-linux matrices in `release.yml` and `nightlies.yml`:

```yaml
- image: ghcr.io/ponylang/ponyc-ci-ubuntu26.04-builder:<tag>
  name: x86-64-unknown-linux-ubuntu26.04
  triple-os: linux-ubuntu26.04
  triple-vendor: unknown
```

The arm64-linux matrix entry in those files uses the **same** `image:` string but `name: arm64-unknown-linux-ubuntu26.04`. Because the structural-exemplar (Alpine 3.23) arm64-linux entries omit `triple-vendor`, this entry omits it too.

(In `release.yml` the arm64-linux job uses `docker run` directly rather than a `container:` job; ignore the surrounding job structure — the matrix entry shape is what matters.)

### 10. Update docs

`INSTALL.md`:
- "Supported Glibc distributions" list (only relevant for distros listed there; Alpine adds use a different section): extend the existing comma-separated Ubuntu line in place. Add the new version to that line, don't add a new bullet.
- If a corresponding Pop!_OS or Linux Mint release based on this Ubuntu version exists at this time, add it to its line and to the PLATFORM string table too. If it doesn't exist yet, skip — those rows can be added in a later PR when they ship.
- PLATFORM string table: add two rows (amd64 and arm64). The existing rows for arm64 Ubuntu use `arm-linux-` (not `arm64-linux-`) — match that historical convention; don't "fix" it. PLATFORM strings (in INSTALL.md) and package names (in RELEASE_PROCESS.md) intentionally use different arch prefixes — match existing rows in each file rather than imposing consistency between them.

`RELEASE_PROCESS.md`:
- "Package names will be:" list: add two lines (alphabetical). Artifact naming always uses the arch-prefixed form, regardless of how the builder image is named:
  - `* ponyc-arm64-unknown-linux-<distro><version>.tar.gz`
  - `* ponyc-x86-64-unknown-linux-<distro><version>.tar.gz`

### 11. Add release note

Create `.release-notes/add-<distro>-<version>.md`:

```markdown
## <Distro> <Version> added as a supported platform

We've added arm64 and amd64 builds for <Distro> <Version>. We'll be building ponyc releases for it until it stops receiving security updates in <EOL year>. At that point, we'll stop building releases for it.
```

This combined-arch form matches the precedent for fresh adds (e.g. `.release-notes/0.60.4.md`). The per-arch form ("X on arm64 added as a supported platform") is for the historical case of adding an arch to an already-supported version — not what this skill covers.

EOL lookup — use **standard support** endpoints (not extended/ESM):
- Ubuntu LTS: <https://ubuntu.com/about/release-cycle> (standard support is 5 years from release; e.g. 24.04 → 2029).
- Ubuntu interim (non-LTS, e.g. 25.04): same release-cycle page; standard support is 9 months. Round up to the year if the EOL falls inside it.
- Alpine: <https://alpinelinux.org/releases/> (typically 2 years).

After this step, sanity-check coverage:

```bash
git diff --stat main
grep -rn "<distro><version>" .github/workflows/ INSTALL.md RELEASE_PROCESS.md .ci-dockerfiles/ .release-notes/
```

`git diff --stat` should show changes only in the files listed in the "File inventory" section above (plus the two new Dockerfile/script files and the new release-notes file) — investigate any unexpected location. The `grep` confirms the placeholder was substituted; note that `build-builder-image.yml` job-key `<distro><version_underscore>` (e.g. `ubuntu26_04`) won't match the dotted form, so check that file separately.

### 12. Squash, push, open PR

```bash
git fetch origin main
git reset --soft origin/main
git add -A
git commit -m "Add <Distro> <Version> as a release target"
git push --force-with-lease

gh pr create --title "Add <Distro> <Version> as a release target" --body "$(cat <<'EOF'
Adds <Distro> <Version> as a supported release target. Builder image, release/nightly/lib-cache workflow entries, INSTALL/RELEASE_PROCESS docs, and a release note are all included.
EOF
)"
```

### 13. Post-merge: Last Week in Pony comment

After the PR merges, find the open LWIP issue and post a comment.

```bash
LWIP=$(gh issue list -R ponylang/ponylang-website \
  --label last-week-in-pony --state open --limit 1 --json number --jq '.[0].number // empty')

if [ -z "$LWIP" ]; then
  echo "No open LWIP issue. Wait until the next one is created (typically within a week) and post then. Don't open a new LWIP issue yourself."
else
  gh issue comment "$LWIP" -R ponylang/ponylang-website --body "$(cat <<'EOF'
<Distro> <Version> has been added as a supported platform for ponyc. Prebuilt binaries are available via [ponyup](https://github.com/ponylang/ponyup). We'll continue building releases for it until <EOL year>, when its upstream security support ends.
EOF
)"
fi
```

The comment substance matches the release note: announcing the new platform with the EOL year. Format is short, single-paragraph prose matching existing LWIP comments.

After this step, the add-a-release-target work is complete.

## Failure modes — when to stop and ask

- **Local Dockerfile build fails inside the image.** Package gone, repo URL invalid, base image not yet published. Don't tweak the package list or "fix forward" without confirmation.
- **GHA builder dispatch fails.** Read run logs with `gh run view <id> --log-failed`. Most causes are the same as above.
- **GHCR tag query returns no result.** Build may not have pushed (auth failure, network error). Check the workflow run logs.
- **arm64 build fails but amd64 succeeds (or vice versa).** Multi-platform `docker buildx` produces a single manifest covering both arches; if either platform fails, the whole build fails and nothing gets pushed. Triage from the failed-platform logs.

## Anti-patterns

- **Don't conflate this with CI test workflows.** PR validation, tier 2, weekly checks, and stress tests are separate concerns. They may also need updating for new distros, but not as part of "add a release target."
- **Don't migrate existing per-arch builders to multi-platform as a side effect.** That's a separate change.
- **Don't skip the smoke test to save time.** A failed GHA multi-platform build is a 30-minute round-trip plus the human-attention cost of triaging it. A failed local build is 30 seconds.
- **Don't run `build-and-push.bash` locally.** It pushes. The smoke test is `docker build` only.
