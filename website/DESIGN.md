# BRUCE website design and implementation plan

**Status: IMPLEMENTED AND VERIFIED — website acceptance is recorded in README.md.**

The requested deliverable is an English marketing and documentation site for
BRUCE_raw, generated as static files for GitHub Pages. Source documents remain
authoritative. Existing working-copy changes are preserved.

Eleventy supplies directory-index pages and a local preview without a browser
framework. A build-time content adapter reads an explicit manifest of repository
files, renders Markdown with raw HTML disabled, parses CSV as strings, and escapes
technical previews. It rewrites selected document links to website URLs and other
repository references to GitHub. No source file is executed as a template.

The visual system uses warm paper, green ink, a fine grid, large editorial type,
and a mathematical illustration derived from the actual affine training data.
The homepage introduces bounded research, replay, exact arithmetic, and explicit
control; it includes authentic commands and makes proof boundaries visible.
Documentation uses grouped navigation, heading anchors, a page contents list,
and progressive enhancement. Search loads a generated index only on its page.

## Implementation sequence

- [x] Content pipeline: manifest, safe Markdown, relative links, CSV/JSON/YAML
  previews, downloads, translation hashes, tests for unsafe input and URL bases.
- [x] Site: shared layout, homepage, docs, search, 404, responsive styling,
  keyboard-operable enhancements, canonical metadata and sitemap.
- [x] Delivery: locked dependencies, isolated Pages workflow, maintenance guide,
  production crawl and browser tests on project and root deployments.

## Acceptance boundaries

Test project paths at `/My-Bruce-AI/` and domain-root paths at `/`. Crawl generated
links, fragments, assets and downloads. Compare downloads byte-for-byte with
allowlisted inputs. Test direct nested visits, mobile overflow, keyboard search,
CSV filtering/sorting and no-JavaScript navigation. Build without changing C++
or existing CI. Do not push, enable Pages, or publish.

GitHub repository metadata checked during implementation reports default branch
`main-beta-001` and private visibility. No CNAME or previous Pages setup exists
in the checkout. The default anticipated URL is
`https://chrisfle2003.github.io/My-Bruce-AI/`; this is not a verified live site.
