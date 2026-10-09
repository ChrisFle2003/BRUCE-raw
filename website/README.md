# BRUCE website

**IMPLEMENTED AND VERIFIED** — static production builds, generated link and
asset checks, and browser verification are described below. This status concerns
the website, not a fresh verification of the C++ core.

**IMPLEMENTED, BUT NOT CURRENTLY VERIFIED** — the GitHub Pages workflow is
prepared; its hosted execution and a live deployment have not been tested.
Nothing has been pushed, Pages has not been enabled, and no publication has
been triggered by this work.

## Local development

Use Node.js 24 and the exact pnpm version declared in `package.json`
(`11.19.0`). There was no JavaScript package manager in the original repository;
the website uses its own package manifest and lockfile, independent of CMake.

From the repository root:

```sh
cd website
pnpm install --frozen-lockfile
pnpm dev
```

The default deployment prefix is `/My-Bruce-AI/`. For Eleventy's development
server, use a root URL during development:

```sh
SITE_URL=http://localhost:8080/ pnpm dev
```

For a production build and a preview with the actual prefix:

```sh
pnpm build
pnpm check
pnpm check:package
pnpm preview
```

Open `http://127.0.0.1:8080/My-Bruce-AI/`. The preview server serves only `_site`,
uses real directory indexes, and returns the static 404 page for missing routes.
It is a local development tool, not a production server.

## Build and tests

```sh
pnpm test
pnpm build
pnpm check
pnpm check:package
pnpm exec playwright install chromium
pnpm test:browser
```

On a fresh Linux CI machine, install browser dependencies with
`pnpm exec playwright install --with-deps chromium`.

The content tests cover unsafe Markdown, heading IDs, deployment URLs, relative
links, quoted CSV fields, multiline Unicode, missing values, bounded previews,
and inert technical previews. The production crawl checks every generated HTML
page, local links and fragments, metadata, assets, the search index, sitemap,
and byte-identical downloads. The package check repeats the crawl after applying
the Pages uploader’s archive exclusions. It also checks that downloads match the allowlist.
Browser tests cover direct nested loads and refreshes, search, clipboard copying,
CSV controls, keyboard navigation, no-JavaScript content, reduced motion, and
360/768/1440-pixel layouts. Screenshots are local artifacts in `test-results/`.

To verify root-domain hosting, use the same URL for every command:

```sh
SITE_URL=https://example.org/ pnpm build
SITE_URL=https://example.org/ pnpm check
SITE_URL=https://example.org/ pnpm test:browser
```

Rebuild without `SITE_URL` afterward to restore the repository's default prefix.
The generated `_site` directory is ignored. If removing a manifest entry, start
with an empty `_site` directory: the checker rejects leftover downloads so an
old file cannot silently be published. CI always starts from a clean checkout.

## GitHub Pages deployment

The existing core workflow is unchanged. The separate
[Pages workflow](../.github/workflows/pages.yml) installs locked dependencies,
runs content tests, builds and crawls the site, runs Chromium checks, uploads
`website/_site` with `actions/upload-pages-artifact`, then deploys with
`actions/deploy-pages`. Pull requests only build and test.

GitHub repository metadata read on 9 October 2026 reported the default branch as
`main-beta-001`, which is used explicitly for push and deployment conditions.
If the default branch changes, update those conditions and `branch` in
`lib/content.mjs` (the source-link reference).

When ready to publish:

1. Ensure the intended website files and their source documentation are on
   `main-beta-001`. This site reads the working tree at build time; pre-existing
   documentation edits are not owned by the website change.
2. In **Settings → Pages → Build and deployment**, choose **GitHub Actions** as
   the source. The workflow deliberately does not enable Pages itself.
3. Confirm the `github-pages` environment permits the default branch. Then a
   qualifying push or an explicitly triggered workflow run can deploy.

The anticipated URL is `https://chrisfle2003.github.io/My-Bruce-AI/`. It is not a
verified live URL. The repository is private: GitHub Pages availability depends
on the account's plan, and repository source links require access. Published
site files are the explicit allowlisted selection; review the intended audience
before enabling publication. See GitHub's
[custom workflow documentation](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

The production build uses Pages' configured `base_url`. An optional repository
variable `SITE_URL` overrides it; use a complete URL including the project path,
for example `https://chrisfle2003.github.io/My-Bruce-AI/`. Use
`https://example.org/` for a root-domain site. Local builds default to the known
project URL. Root or website `CNAME` files are preserved if present; neither
existed when this site was created. Configure any custom domain in Pages too.
Canonical URLs, navigation, downloads, assets, search results, and sitemap all
use the same deployment configuration.

## Content selection and maintenance

[content/manifest.json](content/manifest.json) is the publication allowlist.
There is no recursive documentation discovery. Each entry has a repository
`source`, stable website `url`, English `title`, `group`, and `description`.
Markdown is the default; `kind` may be `csv`, `json`, `yaml`, or `text`.
CSV entries can set `delimiter` explicitly (default `,`). Use an explicit
`download` path for sources in hidden directories: the CI YAML is published as
`workflows/ci.yml` because the Pages packager excludes `.github`. Hidden download
paths are rejected.

- English Markdown is read directly at build time. Edit the original source;
  do not keep a second copy in the website.
- For a new document, audit it for publication, add one manifest entry, build,
  and run the checker. Only allowlisted inputs become downloads.
- Internal links to selected documents resolve to site pages, including known
  anchors. Links to unselected files or untranslated sections lead to the
  original GitHub source. This deliberately avoids broken or invented pages.
- The site currently imports no repository images because none were available.
  The brand mark, social card, and homepage plot are site-owned vector assets;
  the plot uses the affine example data, not a fabricated application screenshot.
- Source HTML is disabled. Markdown never executes template directives, MDX,
  scripts, or code fences. JSON and YAML are parsed as data; technical previews
  are escaped and syntax highlighted. YAML executable tags are rejected.
- CSV values remain strings. The parser handles quoted fields and Unicode;
  sorting changes only the view and uses natural string order, so fraction
  sorting is not a rational arithmetic comparison. Filtering matches within
  each cell. Previews stop at 100 rows; downloads preserve the full file bytes.
- Technical previews stop at 100 lines. Large or sensitive files should remain
  excluded; add a bounded, reviewed public reference only when useful.

Do not add `.env`, credentials, private keys, trust stores, local run directories,
build artifacts, internal datasets, or test-fixture secrets. The site currently
publishes four small CSVs, the rational-power example's three JSON files, one
rules file, the core CI YAML, license, and selected documentation. It does not
publish the repository wholesale or treat historical plans as present features.

## Translation maintenance

Four German references have **selected English adaptations**, not full
translations. The site labels their scope and links to the original:

| Original | English adaptation |
| --- | --- |
| `docs/cli/commands.md` | `content/en/commands.md` |
| `docs/cli/formats.md` | `content/en/formats.md` |
| `docs/cli/results-and-control.md` | `content/en/results.md` |
| `docs/cli/local-files.md` | `content/en/local-files.md` |

Each manifest entry stores `sourceHash`, the original's SHA-256. A changed
source fails the build. Review the source diff, update the English adaptation,
retain literal commands/identifiers/data, then update that entry's hash using
`sha256sum ../docs/cli/<filename>`. Do not automatically refresh hashes without
reviewing the translation. Existing German files are never rewritten by the site.
The source remains authoritative and includes specialist sections not selected
for the website; the source repository may require authentication.

The generated CLI files `source-index.md`, `examples.json`, and `validation.md`
are not modified by this website. Their existing temporary-copy verification
workflow remains authoritative.

## Architecture

[Eleventy](https://www.11ty.dev/docs/) was chosen because it emits static directory
indexes without requiring a browser framework or runtime server. `lib/catalog.mjs`
loads the selected sources, `lib/content.mjs` renders them safely, and
`lib/layout.mjs` supplies consistent page chrome. The `.11ty.js` templates produce
the homepage, documentation, search page, sitemap, and 404 page.

The site has no remote fonts, analytics, backend, database, or runtime secrets.
The optional contact page submits directly to Formspree only when configured;
see [contact setup and verification](CONTACT.md). One small browser script progressively adds code-copy buttons, table
controls, and local search. Core pages, navigation, code, and data are ordinary
HTML. Search loads only its generated local index. The SVG social card source
is in `src/assets/social.svg`; its 1200×630 PNG is committed alongside it for
social crawlers and should be re-exported if the card changes.

## Initial website acceptance record — 9 October 2026

**IMPLEMENTED AND VERIFIED:** 24 HTML pages; 20 byte-identical public downloads;
1,261 generated references checked at both `/My-Bruce-AI/` and `/`; six content
tests; eight Chromium scenarios for each deployment base. Desktop/mobile
screenshots were visually reviewed. The Pages archive is independently unpacked
and crawled, catching files the uploader excludes. The package installation
was checked with the frozen lockfile. `git diff --check` and local website
Markdown links were checked.

The existing `build-cli/bruce` binary reports `0.102.0`. The homepage research
example ran with replay verification, returned `VERIFIED_ON_SAMPLES` with
`general_proof: false`, and its bundle replay returned the same domain finding.
The full C++ suite was not rebuilt for this website-only change. The supplied
`with-host-deps.sh` wrapper reports a missing `build-host-deps/root` directory;
the existing binary runs directly. CMake and the core CI workflow are unchanged.

**OPEN DECISION / UPSTREAM DEPENDENCY GAP:** the final production-dependency
audit still reports one high-severity denial-of-service advisory for `braces`
([GHSA-vfj7-8cjw-p6xm](https://github.com/advisories/GHSA-vfj7-8cjw-p6xm)) and one
moderate advisory for `sprintf-js`
([GHSA-hp3w-g68c-fv3c](https://github.com/advisories/GHSA-hp3w-g68c-fv3c)). Both are
transitive Eleventy build-tool dependencies; neither is shipped as browser code.
The registry currently offers `braces` 3.0.3 and `sprintf-js` 1.1.3 as the latest
releases, while the advisories name 3.0.4 and 1.1.4 as fixed versions. No
unpublished version or incompatible major override was added. Keep builds
restricted to reviewed repository content and recheck `pnpm audit --prod` when
upstream releases are available. The Markdown, CSV, and YAML direct dependency
advisories were resolved with published updates. A zero-advisory dependency
status is not claimed.

**IMPLEMENTED, BUT NOT CURRENTLY VERIFIED:** hosted Actions execution, account
eligibility for Pages on this private repository, and the live deployment.
