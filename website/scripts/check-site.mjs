import fs from 'node:fs';
import { downloadPath } from '../lib/content.mjs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { parseHTML } from 'linkedom';
import { getConfig, manifest, repoRoot } from '../lib/catalog.mjs';
const output = path.resolve(process.env.SITE_OUTPUT || '_site');
const config = getConfig();
const htmlFiles = [];
function walk(dir) { for (const entry of fs.readdirSync(dir, { withFileTypes: true })) { const p = path.join(dir, entry.name); if (entry.isDirectory()) walk(p); else if (p.endsWith('.html') && !p.includes('/downloads/')) htmlFiles.push(p); } }
walk(output);
assert.ok(htmlFiles.length > 10, 'Expected generated HTML pages');
const errors = [];
let links = 0;
for (const file of htmlFiles) {
  const { document } = parseHTML(fs.readFileSync(file, 'utf8'));
  const relative = path.relative(output, file).replace(/index\.html$/, '');
  const current = new URL(relative, config.url);
  assert.equal(document.documentElement.lang, 'en');
  assert.ok(document.querySelector('title')?.textContent);
  assert.ok(document.querySelector('meta[name="description"]')?.getAttribute('content'));
  assert.ok(document.querySelector('main#main'));
  assert.equal(document.querySelectorAll('h1').length, 1, `${relative}: exactly one H1`);
  const ids = [...document.querySelectorAll('[id]')].map(e => e.id);
  assert.equal(new Set(ids).size, ids.length, `${relative}: duplicate IDs`);
  for (const el of document.querySelectorAll('[href], [src], meta[property="og:image"]')) {
    const href = el.getAttribute('href') || el.getAttribute('src') || el.getAttribute('content');
    if (!href || /^(mailto:|data:)/.test(href)) continue;
    links++;
    const target = new URL(href, current);
    if (target.origin !== config.origin) continue;
    if (!target.pathname.startsWith(config.base)) { errors.push(`${relative}: outside base: ${href}`); continue; }
    let local = path.join(output, decodeURIComponent(target.pathname.slice(config.base.length)));
    if (fs.existsSync(local) && fs.statSync(local).isDirectory()) local = path.join(local, 'index.html');
    if (!fs.existsSync(local)) { errors.push(`${relative}: missing: ${href}`); continue; }
    if (target.hash && local.endsWith('.html')) {
      const targetDoc = parseHTML(fs.readFileSync(local, 'utf8')).document;
      if (!targetDoc.getElementById(decodeURIComponent(target.hash.slice(1)))) errors.push(`${relative}: missing anchor: ${href}`);
    }
  }
}
for (const item of manifest) assert.deepEqual(fs.readFileSync(path.join(output, 'downloads', downloadPath(item))), fs.readFileSync(path.join(repoRoot, item.source)), `Changed download: ${item.source}`);
const index = JSON.parse(fs.readFileSync(path.join(output, 'search-index.json')));
assert.equal(index.length, manifest.length);
for (const item of index) assert.ok(item.url.startsWith(config.base + 'docs/'));
assert.ok(fs.readFileSync(path.join(output, 'sitemap.xml'), 'utf8').includes(config.url + 'docs/'));
const downloaded = [];
function files(dir) { for (const e of fs.readdirSync(dir, { withFileTypes: true })) { const p = path.join(dir, e.name); if(e.isDirectory()) files(p); else downloaded.push(path.relative(path.join(output,'downloads'),p)); } }
files(path.join(output, 'downloads'));
assert.deepEqual(downloaded.sort(), manifest.map(downloadPath).sort(), 'Only allowlisted files may be published');
if (errors.length) { console.error(errors.join('\n')); process.exit(1); }
console.log(`Checked ${htmlFiles.length} HTML pages, ${links} references, ${manifest.length} byte-identical allowlisted downloads, search index and sitemap at ${config.base}`);
