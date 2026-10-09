import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { configFor, renderMarkdown, renderCsv, previewTechnical } from './content.mjs';
export const siteRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const repoRoot = path.dirname(siteRoot);
export const manifest = JSON.parse(fs.readFileSync(path.join(siteRoot, 'content/manifest.json'), 'utf8'));
export function getConfig() {
  const cnamePath = [path.join(repoRoot, 'CNAME'), path.join(siteRoot, 'CNAME')].find(p => fs.existsSync(p));
  const cname = cnamePath ? fs.readFileSync(cnamePath, 'utf8').trim() : null;
  const config = configFor(process.env.SITE_URL || (cname ? `https://${cname}/` : undefined));
  return { ...config, cname, version: fs.readFileSync(path.join(repoRoot, 'CMakeLists.txt'), 'utf8').match(/project\(BRUCE VERSION ([\d.]+)/)[1] };
}
export function getPages() {
  const config = getConfig();
  const urls = new Set();
  const sources = new Set();
  const pages = manifest.map(item => {
    if (urls.has(item.url) || sources.has(item.source)) throw new Error(`Duplicate manifest entry: ${item.source}`);
    urls.add(item.url); sources.add(item.source);
    const resolved = path.resolve(repoRoot, item.source);
    if (!resolved.startsWith(repoRoot + path.sep) || fs.lstatSync(resolved).isSymbolicLink()) throw new Error(`Invalid content path: ${item.source}`);
    const raw = fs.readFileSync(resolved, 'utf8');
    if (item.translation && crypto.createHash('sha256').update(raw).digest('hex') !== item.sourceHash) throw new Error(`Translation needs review: ${item.source}. Review the English adaptation before updating sourceHash.`);
    const text = item.translation ? fs.readFileSync(path.join(siteRoot, item.translation), 'utf8') : raw;
    return { ...item, raw, text, kind: item.kind || 'markdown', headingIds: [] };
  });
  const context = page => ({ source: page.source, pages, base: config.base, assets: [] });
  // First pass discovers anchors, so cross-document references resolve independently of file order.
  for (const page of pages) {
    if (page.kind === 'markdown') page.headingIds = renderMarkdown(page.text, context(page)).headings.map(h => h.id);
  }
  for (const page of pages) {
    if (page.kind === 'markdown') Object.assign(page, renderMarkdown(page.text, context(page)));
    else {
      page.html = page.kind === 'csv' ? renderCsv(page.raw, page.delimiter || ',') : previewTechnical(page.raw, page.kind);
      page.headings = [];
    }
    page.searchText = page.kind === 'markdown' ? page.text.replace(/[`#*|>]/g, '').replace(/\[([^\]]+)\]\([^)]*\)/g, '$1') : page.raw.slice(0, 16000);
  }
  return pages;
}
