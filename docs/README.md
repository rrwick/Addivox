# Addivox docs

This directory contains the Addivox documentation site, built with [MkDocs](https://www.mkdocs.org/) and [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/).


## Local preview

Run commands from this directory:
```sh
DYLD_LIBRARY_PATH=/opt/homebrew/lib mkdocs serve
```

Then open <http://127.0.0.1:8000/>.


## Build

This command will build a local copy of the docs, including a PDF:
```sh
DYLD_LIBRARY_PATH=/opt/homebrew/lib mkdocs build
```


## Deployment

The site is intended to be hosted at: https://rrwick.github.io/Addivox/

Deploy with:
```sh
DYLD_LIBRARY_PATH=/opt/homebrew/lib mkdocs gh-deploy
```

This builds the site and pushes the generated HTML to the `gh-pages` branch. GitHub Pages should be configured to publish from that branch, using the root directory.


## Dependencies (one-time setup):

```sh
brew install pango
pip install mkdocs-with-pdf
```
