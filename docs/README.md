# Addivox docs

This directory contains the Addivox documentation site, built with [MkDocs](https://www.mkdocs.org/) and [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/).


## Local preview

Run commands from this directory:

```sh
mkdocs serve
```

Then open <http://127.0.0.1:8000/>.

For a one-off build check:

```sh
mkdocs build
```


## Deployment

The site is intended to be hosted at:

<https://rrwick.github.io/Addivox/>

Deploy with:

```sh
mkdocs gh-deploy
```

This builds the site and pushes the generated HTML to the `gh-pages` branch. GitHub Pages should be configured to publish from that branch, using the root directory.
