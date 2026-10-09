import { layout } from '../lib/layout.mjs';
import { getConfig } from '../lib/catalog.mjs';
export default class {
  data() { return { permalink: '404.html' }; }
  render() { return layout({ title: 'Page not found', description: 'This address is outside the published BRUCE documentation. Return to the index or search for a guide.', url: '404.html', body: `<main id="main" class="not-found wrap"><div class="eyebrow">404 / Outside the search space</div><h1>No page at<br>these coordinates.</h1><p>This page may have moved, or the address may be incomplete.</p><div class="hero-actions"><a class="button primary" href="${getConfig().base}docs/">Browse documentation →</a><a class="button secondary" href="${getConfig().base}search/">Search →</a></div></main>` }); }
}
