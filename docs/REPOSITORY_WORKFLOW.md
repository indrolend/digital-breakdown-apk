# Repository workflow

## Source of truth

- `main` is the only long-lived integration branch.
- Active work uses short-lived branches named `agent/*`, `feature/*`, `fix/*`, `release/*`, or `tooling/*`.
- A branch without an active pull request is considered temporary and may be removed after its useful commits are merged or archived.
- Generated binaries, diagnostic sessions, build folders, credentials, local SDK paths, and dependency folders must never be committed.

## Daily synchronization

Before starting work:

```bash
git fetch --prune origin
git switch main
git pull --ff-only origin main
git status --short --branch
```

Start one focused branch:

```bash
git switch -c feature/short-description
```

Before pushing:

```bash
git fetch origin
git rebase origin/main
git diff --check
git status --short
git push --set-upstream origin HEAD
```

Never use `git pull` without understanding whether it will merge or rebase. The recommended local settings are:

```bash
git config pull.ff only
git config fetch.prune true
git config rebase.autoStash true
git config rerere.enabled true
```

## Pull requests

A pull request should represent one coherent change. It must describe:

- user-visible or developer-visible behavior;
- affected platforms;
- state/protocol/schema changes;
- debug-suite registration;
- tests run;
- release or migration implications.

Draft pull requests are acceptable for active work. Superseded and empty pull requests should be closed promptly with a pointer to the active replacement.

Do not stack new development on an old unmerged feature branch unless the dependency is intentional and documented. Prefer merging the prerequisite first or making the base relationship explicit.

## Required checks

The `Continuous Integration` workflow validates:

- whitespace and repository hygiene;
- shared native parity and multiplayer protocol tests;
- multiplayer backend type generation, TypeScript checks, tests, and dry deployment;
- Android debug compilation.

The release workflow separately builds Windows, Android, and universal macOS packages and refuses to assemble a release unless every platform artifact exists.

## Backend

`multiplayer-server/package-lock.json` is authoritative. Use:

```bash
cd multiplayer-server
npm ci
npm run check
npm run deploy:dry
```

Production deployment is an explicit action:

```bash
npm run deploy
```

Do not deploy from a dirty working tree or from an unreviewed branch. Protocol changes must update native serialization, server validation, tests, and the protocol version in the same pull request.

## Shipping

There are two release forms:

1. `latest-native`: rolling packages generated only from `main`.
2. Versioned releases: immutable tags such as `v0.9.1` created after the rolling build has been validated.

A patch release should contain the smallest safe fix, an updated version/changelog entry, relevant regression coverage, and no unrelated refactor.

Release artifacts contain a build manifest and SHA-256 checksums. Never replace a versioned tag or versioned release. Only the rolling `latest-native` release may be replaced.

## Cleanup

After a pull request is merged or closed:

```bash
git fetch --prune origin
git branch --merged main
git branch -d branch-name
```

Use force deletion only after confirming that the commits exist elsewhere:

```bash
git branch -D branch-name
```

Before deleting a remote branch, verify that its pull request is merged, closed as superseded, or intentionally abandoned.

## Recovery

Useful recovery commands:

```bash
git reflog
git log --all --decorate --oneline --graph
git fsck --lost-found
git restore --source=<commit> -- path/to/file
git switch -c recovery/<name> <commit>
```

Do not rewrite `main`, release tags, or published version history.
