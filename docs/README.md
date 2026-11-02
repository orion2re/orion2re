# Documentation portal

The portal is opt-in because configuration reference generation reads native game defaults.
HTML builds require Python 3.10 or newer with the standard `venv` and `ensurepip` modules. CMake creates `<project>/.venv` and installs the committed, hash-pinned lock file with that environment's `python -m pip`; `uv`, Poetry, and other environment managers are not required. Bootstrap first verifies and installs pip 26.2.2 against its committed SHA-256 digest, avoiding the obsolete CA bundle in older `ensurepip` installations. The environment is reused across CMake cache and build-directory recreation.

Configure and build on Linux with:

```sh
cmake --preset docs
cmake --build --preset docs
```

The build preset runs the strict documentation check. To serve the portal locally, run:

```sh
cmake --build out/build/Linux/docs --target docs-serve
```

Generated Markdown and the rendered site remain under the CMake build tree. The reusable Python environment and its lock-file stamp remain in the ignored `<project>/.venv` directory, so recreating the CMake build tree does not reinstall unchanged dependencies. Do not edit generated configuration pages. The `docs-clean` target removes generated Markdown and rendered output while retaining `.venv`.

The historical 1.50.26 manual is committed as categorized Markdown with local images and source provenance. Portal builds do not contact the original manual website.

## Single-file PDF

The opt-in `docs-pdf` target exports the entire portal, including the generated configuration reference, in navigation order. It uses a separate environment at `<build>/docs/pdf-venv`; normal HTML builds do not install PDF dependencies or require rendering libraries.

PDF builds require **Python 3.11+**, Pango, HarfBuzz (including its subset library), and fonts. On Ubuntu 24.04, install these in addition to the native project's build dependencies:

```sh
sudo apt-get install python3-venv libpango-1.0-0 libpangoft2-1.0-0 libharfbuzz-subset0 fonts-dejavu-core
cmake --preset docs
cmake --build --preset docs --target docs-pdf
```

The result is `out/build/Linux/docs/docs/orion2re-documentation.pdf`.

On macOS, install `pango` and `harfbuzz` with Homebrew and configure a separate native documentation build:

```sh
cmake -S . -B out/build/Darwin/docs-pdf -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DORION2RE_BUILD_GAME=OFF \
  -DORION2RE_BUILD_CONFIG_TOOL=ON -DORION2RE_BUILD_DOCUMENTATION=ON \
  -DPython3_EXECUTABLE="$(command -v python3.12)"
cmake --build out/build/Darwin/docs-pdf --target docs-pdf --parallel 4
```

The result is `out/build/Darwin/docs-pdf/docs/orion2re-documentation.pdf`. If the dynamic loader cannot find Homebrew's rendering libraries, set `DYLD_FALLBACK_LIBRARY_PATH="$(brew --prefix)/lib"` for the build.

The build checks that the PDF has pages and bookmarks, is unencrypted, and contains no embedded-file name tree, JavaScript name tree, or automatic open action. Rendering runs in a subprocess with a ten-minute timeout and, on Linux, a 4 GiB address-space limit. Resource fetching rejects network access and file URLs outside the generated site, including symlink escapes; a blocked resource fails the build rather than silently publishing an incomplete PDF.

These restrictions are **not a sandbox for arbitrary repository code**: MkDocs and PDF hooks execute Python. Build untrusted pull requests only on disposable runners without secrets or release-write credentials. The CI documentation job retains read-only repository permissions; the separate release publisher attaches the validated PDF without executing the checkout. Native libraries and bundled wheel libraries still require OS-level security updates.

### Dependencies and maintenance

The PDF exporter is [mkdocs-to-pdf 0.12.1](https://github.com/domWalters/mkdocs-to-pdf), maintained by Dom Walters and Hideyuki Kato (MIT), using [WeasyPrint 70.0](https://github.com/Kozea/WeasyPrint), maintained by CourtBouillon (BSD-3-Clause). [pypdf 6.21.4](https://github.com/py-pdf/pypdf) (BSD-3-Clause) validates the artifact. These are build tools, not game runtime dependencies. Installed wheels retain their upstream license notices.

The exporter still requires the native MIT-licensed LibSass compiler, which [reached end-of-life](https://sass-lang.com/blog/libsass-is-end-of-life/). Its use is confined to the PDF environment; do not install the exporter's `dev`, `samples`, or `all` extras. JavaScript rendering is disabled, so Chrome and Node.js are not required.

Both locks are installed with `--require-hashes --only-binary=:all:`. To regenerate them, use a separate tooling environment with `uv >=0.13.4` (needed only for lock maintenance):

```sh
uv pip compile docs/requirements.in --constraint docs/requirements.lock \
  --generate-hashes --no-annotate --no-header --universal --python-version 3.10 \
  --output-file docs/requirements.lock
uv pip compile docs/requirements-pdf.in --constraint docs/requirements.lock \
  --generate-hashes --no-annotate --no-header --universal --python-version 3.11 \
  --output-file docs/requirements-pdf.lock
python3 tools/audit_documentation.py docs/requirements.lock docs/requirements-pdf.lock
```

When updating an existing pin, update its constraint as well. Review the complete lock diff and audit before installing. The OSV check fails closed on known advisories or request failures; it does not replace provenance review or native-library scanning.

Run the focused regression tests using the PDF environment:

```sh
out/build/Linux/docs/docs/pdf-venv/bin/python -m unittest discover -s tests/documentation -v
```

## CI artifacts and releases

Every documentation job builds the HTML portal and PDF from the checked-out revision, then uploads two separate files, retained for 30 days:

- `orion2re-documentation-main.zip`: the complete offline HTML portal, including generated references, images, styles, and search assets. Extract it and open `index.html` at the archive root.
- `orion2re-documentation-main.pdf`: the standalone PDF, uploaded directly without a ZIP wrapper and not included in the portal ZIP.

Published releases use the release tag instead of `main` in both filenames. Both documentation assets are attached alongside the three platform ZIPs. Publication requires all five assets and refuses to replace existing release assets.

To package a locally built portal:

```sh
python3 tools/package_documentation.py out/build/Linux/docs/docs/site out/orion2re-documentation-main.zip
```

Packaging validates the ZIP CRCs and rejects missing entry pages, symlinks, unsafe filenames, existing output archives, and accidentally included PDFs. Only the generated HTML site is packaged, not its virtual environments or build tools.