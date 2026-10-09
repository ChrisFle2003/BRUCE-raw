import { layout } from '../lib/layout.mjs';
import { getConfig } from '../lib/catalog.mjs';
export default class {
  data() { return { permalink: 'search/index.html' }; }
  render() {
    const { base } = getConfig();
    return layout({ title: 'Search documentation', description: 'Search the selected BRUCE documentation and data references locally in your browser.', url: 'search/', docs: true, body: `<main id="main" class="search-page wrap"><div class="eyebrow">Find your next step</div><h1>Search the documentation.</h1><p class="doc-lead">Commands, concepts, and data files. Search runs locally in your browser.</p><form id="search-form" role="search"><label for="search-input">Search terms</label><div class="search-field"><input id="search-input" name="q" type="search" placeholder="Try “replay”, “rational”, or “holdout”" autocomplete="off" disabled><button type="submit" class="button primary" disabled>Search →</button></div></form><p id="search-status" role="status" aria-live="polite">Loading search…</p><noscript><p>Search needs JavaScript. All guides remain available in the <a href="${base}docs/">documentation index</a>.</p></noscript><div id="search-results"></div><p class="search-fallback">Prefer to browse? <a href="${base}docs/">Open all documentation →</a></p></main>` });
  }
}
