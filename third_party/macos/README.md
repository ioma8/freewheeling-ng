# macOS Dependencies

This directory defines the macOS dependency input for local builds and CI.

- `Brewfile` is the single source of truth for required Homebrew formulae
- `scripts/bootstrap-macos.sh` installs exactly that Brewfile
- `scripts/package-macos-dylibs.sh` vendors the resulting non-system dylibs
  into `fweelin.app/Contents/Frameworks` and rewrites install names so the app
  does not depend on `/opt/homebrew` at runtime

The current flow is reproducible at the package-manager level and produces a
self-contained `.app` bundle. It does not yet build third-party libraries from
source under a repo-managed prefix.
