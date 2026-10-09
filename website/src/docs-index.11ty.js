import { layout, sidebar } from '../lib/layout.mjs';
import { getConfig } from '../lib/catalog.mjs';
import { escape } from '../lib/content.mjs';
export default class {
  data() { return { permalink: 'docs/index.html' }; }
  render({ catalog }) {
    const { base } = getConfig();
    return layout({ title: 'Documentation', description: 'Build, run, and inspect BRUCE. Source-backed guides, CLI conventions, architecture, and downloadable sample data.', url: 'docs/', docs: true, body: `<div class="docs-shell docs-index">${sidebar(catalog)}<main id="main" class="doc-main"><div class="eyebrow">The developer field guide</div><h1>From first run<br>to reproducible result.</h1><p class="doc-lead">Understand the core, choose your inputs, and follow the evidence. These pages are built from selected repository files.</p><div class="doc-callout"><span class="status-dot"></span><div><strong>Start with the boundaries.</strong><p>A successful command is not automatically a complete search or a mathematical proof. Each source document states its evidence status.</p></div></div>${[...new Set(catalog.map(p => p.group))].map(group => `<section class="doc-category"><h2>${group}</h2><div class="doc-card-grid">${catalog.filter(p => p.group === group).map(p => `<a class="doc-card" href="${base}${p.url}"><span class="card-arrow" aria-hidden="true">↗</span><h3>${escape(p.title)}</h3><p>${escape(p.description)}</p><small>${p.translation ? 'English adaptation' : p.kind === 'markdown' ? 'Repository documentation' : p.kind.toUpperCase() + ' · source file'}</small></a>`).join('')}</div></section>`).join('')}</main></div>` });
  }
}
