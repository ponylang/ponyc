---
name: add-linux-release-target
description: Load when adding a new Ubuntu (or other per-arch-builder Linux distro) version as a ponyc release target. Covers builder Dockerfile creation, image build dispatch and tag discovery, workflow updates, docs, release notes, and the post-merge Last Week in Pony announcement.
disable-model-invocation: false
---

# Adding a New Linux Distro/Version as a ponyc Release Target

## Scope

Use this skill when adding a new distro/version (e.g. Ubuntu 26.04) so ponyc produces release and nightly artifacts users can install via ponyup.

The procedure assumes the **per-arch builder pattern** — separate `.ci-dockerfiles/x86-64-unknown-linux-<distro><version>-builder/` and `.ci-dockerfiles/arm64-unknown-linux-<distro><version>-builder/` directories. Ubuntu uses this pattern (and historically Alpine ≤3.21 did too). Alpine 3.22+ uses a single multi-arch builder dir (`.ci-dockerfiles/<distro><version>-builder/`) and is not covered here — for those adds, look at the existing Alpine 3.22/3.23 layout and adapt.

**Out of scope:**
- CI test-only workflows: `pr-ponyc.yml`, `ponyc-tier2.yml`, `ponyc-weekly-checks.yml`, `stress-test-*.yml`. These reference Linux images for testing ponyc itself, not for producing release artifacts. Don't touch them.
- `pr-docker-image-validation.yml` validates the user-facing `.dockerfiles/{nightly,release}/Dockerfile` images (Alpine-based). Not affected by adding an Ubuntu release target.
- Removing EOL'd distros. Different procedure, not covered here.

## Guiding facts

- **Purely additive.** Older supported versions stay until their own upstream EOL. Don't propose dropping anything as part of this work.
- **Every available architecture.** Include every arch for which a GitHub Actions runner exists. The historical asymmetry (e.g. Ubuntu 22.04 has no arm64 builder) is because arm64 GHA runners didn't exist when those targets were added — not a policy. As of writing, both `ubuntu-latest` and `ubuntu-NN.NN-arm` runners exist, so add both x86-64 and arm64 by default.
- **GHCR builder packages are repo-scoped.** No manual "make public" step is needed for new packages — same-repo CI pulls them automatically.
- **Mental model.** Two builder Docker images (one per arch) get pushed to GHCR with a date-stamp tag. Workflow files (`release.yml`, `nightlies.yml`, `update-lib-cache.yml`) reference those images by exact tag. Adding a new release target = create the Dockerfiles, build/push the images, then thread the resulting tag into every workflow that consumes them, plus update the user-facing docs.

## File inventory

New files (two per arch, four total per distro/version):
- `.ci-dockerfiles/x86-64-unknown-linux-<distro><version>-builder/Dockerfile`
- `.ci-dockerfiles/x86-64-unknown-linux-<distro><version>-builder/build-and-push.bash`
- `.ci-dockerfiles/arm64-unknown-linux-<distro><version>-builder/Dockerfile`
- `.ci-dockerfiles/arm64-unknown-linux-<distro><version>-builder/build-and-push.bash`

Existing files to edit:
- `.github/workflows/build-builder-image.yml` — dropdown options + two job blocks
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

Throughout, the **canonical exemplar** is the most recent existing same-distro per-arch builder. Identify it empirically:

```bash
ls .ci-dockerfiles/ | grep '^x86-64-unknown-linux-<distro>' | sort -V | tail -1
```

For Ubuntu adds at the time this skill was written, the canonical exemplar is `ubuntu24.04`. Substitute its values in the templates below.

### 1. Branch

```bash
git checkout main && git pull
git checkout -b add-<distro>-<version>-release-target
```

### 2. Create builder Dockerfiles and scripts

Copy the canonical-exemplar same-distro per-arch builders and adjust the version. Crucial: copy x86-64 from the existing x86-64 builder and arm64 from the existing arm64 builder — **do not** copy across arches. The `build-and-push.bash` scripts differ by arch:

- x86-64 uses `docker build --pull` followed by `docker push`.
- arm64 uses `docker buildx create / build / rm` with `--platform linux/arm64 --provenance false --sbom false --pull --push` (works around an arm64 buildx push bug).

Both Dockerfiles use the same multi-arch base image (e.g. `FROM ubuntu:26.04`); Docker pulls the right architecture for the runner.

Verify the copy:
- `Dockerfile`: in the common case, only the `FROM` line should change (e.g. `FROM ubuntu:24.04` → `FROM ubuntu:26.04`).
- `build-and-push.bash`: only the `NAME` value changes within each arch's script.

Don't modify the apt package list, the `LABEL`, the user setup, or anything else without a concrete reason. If a package name has changed in the new distro version, the local smoke test in step 3 will surface it — don't pre-emptively edit.

### 3. Local smoke test (x86-64 only)

Before pushing or dispatching anything, build the x86-64 Dockerfile locally:

```bash
docker build --pull .ci-dockerfiles/x86-64-unknown-linux-<distro><version>-builder/
```

`--pull` is important — it forces Docker to fetch the latest base image rather than use a stale local cache.

This is a smoke test, not a push — do **not** run `build-and-push.bash` locally (that would push to GHCR with today's date stamp).

If the build fails because Docker isn't running or apt-get can't reach the network, that's an environment problem — fix it and retry, don't escalate. If the build fails inside the image (apt package gone, repo URL invalid for the new distro, base image not yet published, etc.): **STOP**. Surface the error to the human. Don't retry. Don't "fix" by tweaking the package list without confirmation — that's a design discussion.

Skip the arm64 local build. Native arm64 hardware isn't assumed; cross-build via qemu is slow and rarely surfaces issues that x86-64 didn't. The GHA dispatch in step 6 is the real arm64 verification.

### 4. Update `build-builder-image.yml`

Two edits, both mechanical:

**a. Add to the dropdown options** (alphabetical across the whole list):

```yaml
options:
  ...
  - arm64-unknown-linux-<distro><version>-builder
  ...
  - x86-64-unknown-linux-<distro><version>-builder
```

**b. Add two job blocks** by copying the existing canonical-exemplar jobs (e.g. `arm64-unknown-linux-ubuntu24_04-builder` and `x86-64-unknown-linux-ubuntu24_04-builder`) and substituting the version. Place each new job alphabetically among the same-arch sibling jobs.

The job map key replaces `.` with `_` (YAML map keys can't contain `.`): a version of `26.04` becomes `26_04` in the job key only — the `if:` filter value, the `name:` field, and the `bash` script path all keep the dot form. Reference shape:

```yaml
x86-64-unknown-linux-<distro><version_underscore>-builder:
  if: ${{ github.event.inputs.builder-name == 'x86-64-unknown-linux-<distro><version>-builder' }}
  runs-on: ubuntu-latest

  name: x86-64-unknown-linux-<distro><version>-builder
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
      run: bash .ci-dockerfiles/x86-64-unknown-linux-<distro><version>-builder/build-and-push.bash
```

The arm64 job is the same except `runs-on: ubuntu-24.04-arm` and the arch prefix in the keys, filter, name, and bash path. Don't touch the `runs-on` values — they refer to runner labels GitHub provides, which are independent of what we're *building for*.

### 5. Push branch (WIP commit is fine)

```bash
git add .ci-dockerfiles .github/workflows/build-builder-image.yml
git commit -m "WIP: add <distro> <version> builders"
git push -u origin add-<distro>-<version>-release-target
```

### 6. Dispatch builder workflows from the branch

GHA `workflow_dispatch` runs the workflow file from the chosen ref, so the new dropdown options become available when targeting the branch.

```bash
gh workflow run build-builder-image.yml \
  --ref add-<distro>-<version>-release-target \
  -f builder-name=x86-64-unknown-linux-<distro><version>-builder

gh workflow run build-builder-image.yml \
  --ref add-<distro>-<version>-release-target \
  -f builder-name=arm64-unknown-linux-<distro><version>-builder
```

If dispatch returns 403, the `gh` token lacks `workflow` scope — see "Before you begin."

### 7. Wait for both builds to finish

Builds take ~10–30 minutes per arch. The two newest runs on the branch are the dispatches you just made; verify they're queued or in-progress, then watch each:

```bash
gh run list --workflow=build-builder-image.yml \
  --branch=add-<distro>-<version>-release-target \
  --limit=2 --json databaseId,status,conclusion,name --jq '.[].databaseId' \
  | xargs -n1 -I{} gh run watch {} --exit-status
```

If either build fails: **STOP**. Read the failed logs with `gh run view <id> --log-failed`, surface the failure to the human. A common cause is an apt repo or package change for the new distro — that's a discussion, not something to silently work around. Do not proceed with only one arch's image built; both must update together.

### 8. Discover the published tags

Each successful build pushes `ghcr.io/ponylang/<package>:YYYYMMDD`. Query GHCR per package — the two arches' tags can in principle differ if the build straddled UTC midnight.

```bash
gh api -X GET /orgs/ponylang/packages/container/ponyc-ci-x86-64-unknown-linux-<distro><version>-builder/versions \
  --jq '[.[].metadata.container.tags[]] | map(select(test("^[0-9]{8}$"))) | max'

gh api -X GET /orgs/ponylang/packages/container/ponyc-ci-arm64-unknown-linux-<distro><version>-builder/versions \
  --jq '[.[].metadata.container.tags[]] | map(select(test("^[0-9]{8}$"))) | max'
```

The `select(test("^[0-9]{8}$"))` filter restricts to date-form tags (rejects `latest` or other future tags); `max` picks the most recent. Record the tag for each arch independently.

If the API returns 403, the `gh` token lacks `read:packages` — see "Before you begin."

### 9. Apply tags to workflow files

Add new matrix entries alongside the existing same-distro entries. Use the discovered tag from step 8 for each arch.

**Copy the existing canonical-exemplar entry from the file you're editing** — don't impose a single template across files. The current files are inconsistent (e.g. `release.yml` arm64 Ubuntu has `triple-vendor: unknown`; `nightlies.yml` arm64 Ubuntu omits it). Match the existing same-distro entry in each file and just substitute the package name and tag.

Files to edit:
- `.github/workflows/release.yml` — `x86_64-linux` and `arm64-linux` matrices.
- `.github/workflows/nightlies.yml` — `x86_64-linux` and `arm64-linux` matrices.
- `.github/workflows/update-lib-cache.yml` — `x86_64-linux` and `arm64-linux` matrices (simpler shape: just `image:`).

(In `release.yml` the arm64-linux job uses `docker run` directly rather than a `container:` job; ignore the surrounding job structure — the matrix entry shape is what matters.)

### 10. Update docs

`INSTALL.md`:
- "Supported Glibc distributions" list: extend the existing comma-separated Ubuntu line in place. The current line reads `- Ubuntu 22.04, 24.04`; add the new version to that line, don't add a new bullet.
- If a corresponding Pop!_OS or Linux Mint release based on this Ubuntu version exists at this time, add it to its line and to the PLATFORM string table too. If it doesn't exist yet, skip — those rows can be added in a later PR when they ship.
- PLATFORM string table: add two rows (amd64 and arm64). The existing rows use `arm-linux-` (not `arm64-linux-`) for arm64 Ubuntu — match that historical convention; don't "fix" it. PLATFORM strings (in INSTALL.md) and package names (in RELEASE_PROCESS.md) intentionally use different arch prefixes — match existing rows in each file rather than imposing consistency between them.

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

This combined-arch form matches the precedent for fresh adds (e.g. `.release-notes/0.60.0.md` "Alpine 3.20 added as a supported platform"). The per-arch form ("X on arm64 added as a supported platform") is for the historical case of adding an arch to an already-supported version — not what this skill covers.

EOL lookup — use **standard support** endpoints (not extended/ESM):
- Ubuntu LTS: <https://ubuntu.com/about/release-cycle> (standard support is 5 years from release; e.g. 24.04 → 2029)
- Alpine: <https://alpinelinux.org/releases/> (typically 2 years)

After this step, sanity-check coverage:

```bash
grep -rn "<distro><version>" .github/workflows/ INSTALL.md RELEASE_PROCESS.md .ci-dockerfiles/ .release-notes/
git diff --stat main
```

Confirm `git diff --stat` shows changes only in the files listed in the "File inventory" section above (plus the four new Dockerfile/script files and the new release-notes file). The `grep` confirms the placeholder was substituted everywhere it should be.

### 12. Squash, push, open PR

```bash
git fetch origin main
git reset --soft origin/main
git add -A
git commit -m "Add <Distro> <Version> as a release target"
git push --force-with-lease

gh pr create --title "Add <Distro> <Version> as a release target" --body "$(cat <<'EOF'
Adds <Distro> <Version> as a supported release target. Builder images, release/nightly/lib-cache workflow entries, INSTALL/RELEASE_PROCESS docs, and a release note are all included.
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

- **Local Dockerfile build fails inside the image.** Apt package gone, repo URL invalid, base image not yet published. Don't tweak the package list or "fix forward" without confirmation.
- **GHA builder dispatch fails.** Read run logs with `gh run view <id> --log-failed`. Most causes are the same as above.
- **GHCR tag query returns no result.** Build may not have pushed (auth failure, network error). Check the workflow run logs.
- **Tags differ unexpectedly between arches.** UTC midnight straddle is the benign case — record both tags independently and proceed. If something else is going on, investigate before committing.
- **One arch builds but the other fails.** Don't proceed with partial state; both arches must update together. Triage the failure.

## Anti-patterns

- **Don't conflate this with CI test workflows.** PR validation, tier 2, weekly checks, and stress tests are separate concerns. They may also need updating for new distros, but not as part of "add a release target."
- **Don't backfill arm64 on older versions.** If Ubuntu 22.04 lacks an arm64 builder, that's intentional — the project does not retroactively add arch coverage to existing release targets.
- **Don't skip the smoke test to save time.** A failed GHA build is a 30-minute round-trip plus the human-attention cost of triaging it. A failed local build is 30 seconds.
- **Don't run `build-and-push.bash` locally.** It pushes. The smoke test is `docker build` only.
