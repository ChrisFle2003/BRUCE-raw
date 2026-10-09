import path from 'node:path';
import MarkdownIt from 'markdown-it';
import hljs from 'highlight.js';
import { parse } from 'csv-parse/sync';
import { parseDocument } from 'yaml';

export const repository = 'https://github.com/ChrisFle2003/My-Bruce-AI';
export const branch = 'main-beta-001';
export const escape = value => String(value).replace(/[&<>"']/g, c => ({ '&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;' }[c]));
export const slug = text => text.toLowerCase().replace(/[^\p{L}\p{N}\s_-]/gu, '').replace(/\s/g, '-');
export function configFor(input = 'https://chrisfle2003.github.io/My-Bruce-AI/') {
  const url = new URL(input);
  if (!['https:', 'http:'].includes(url.protocol) || url.search || url.hash || url.username || url.password) throw new Error('SITE_URL must be an HTTP(S) site root without credentials, query or fragment.');
  const base = `${url.pathname.replace(/\/+$/, '')}/`;
  return { base, origin: url.origin, url: url.origin + base };
}
export function sourceUrl(source, fragment = '') {
  return `${repository}/blob/${branch}/${source.split('/').map(encodeURIComponent).join('/')}${fragment}`;
}
export function resolveLink(href, ctx, image = false) {
  if (/^(https?:|mailto:|#)/i.test(href)) return href;
  if (/^[a-z][a-z\d+.-]*:|^\/\//i.test(href)) throw new Error(`Unsupported link: ${href}`);
  const [file, fragment = ''] = href.split('#');
  const [filename, query] = file.split('?');
  const target = path.posix.normalize(path.posix.join(path.posix.dirname(ctx.source), decodeURIComponent(filename)));
  if (target.startsWith('../') || target.split('/').some(p => p.startsWith('.') && p !== '.github')) throw new Error(`Non-public source link: ${href}`);
  const suffix = `${query ? '?' + query : ''}${fragment ? '#' + fragment : ''}`;
  if (image) {
    if (!ctx.assets?.includes(target)) throw new Error(`Image must be explicitly allowlisted: ${target}`);
    return `${ctx.base}downloads/${target}${suffix}`;
  }
  const page = ctx.pages.find(p => p.source === target);
  if (page && (!fragment || page.headingIds?.includes(decodeURIComponent(fragment)))) return `${ctx.base}${page.url}${suffix}`;
  return sourceUrl(target, suffix);
}
export function renderMarkdown(text, ctx) {
  const headings = [];
  const counts = new Map();
  const md = new MarkdownIt({ html: false, linkify: false, typographer: false, highlight: (code, lang) => {
    return hljs.getLanguage(lang) ? hljs.highlight(code, { language: lang }).value : escape(code);
  }});
  const originalLink = md.renderer.rules.link_open || ((tokens, i, options, env, self) => self.renderToken(tokens, i, options));
  md.renderer.rules.link_open = (tokens, i, options, env, self) => {
    tokens[i].attrSet('href', resolveLink(tokens[i].attrGet('href'), ctx));
    return originalLink(tokens, i, options, env, self);
  };
  const originalImage = md.renderer.rules.image;
  md.renderer.rules.image = (tokens, i, options, env, self) => {
    tokens[i].attrSet('src', resolveLink(tokens[i].attrGet('src'), ctx, true));
    tokens[i].attrSet('loading', 'lazy');
    return originalImage(tokens, i, options, env, self);
  };
  md.core.ruler.push('heading-ids', state => {
    state.tokens.forEach((token, i) => {
      if (token.type !== 'heading_open') return;
      const label = state.tokens[i + 1].children.map(t => t.content).join('');
      const key = slug(label);
      const count = counts.get(key) || 0;
      counts.set(key, count + 1);
      const id = key + (count ? `-${count}` : '');
      token.attrSet('id', id);
      headings.push({ id, text: label, level: Number(token.tag.slice(1)) });
    });
  });
  const originalHeading = md.renderer.rules.heading_close;
  md.renderer.rules.heading_close = (tokens, i, options, env, self) => {
    const id = tokens[i - 2].attrGet('id');
    return `<a class="heading-anchor" href="#${escape(id)}" aria-label="Link to this section">#</a>${originalHeading ? originalHeading(tokens, i, options, env, self) : self.renderToken(tokens, i, options)}`;
  };
  let html = md.render(text);
  html = html.replace(/<blockquote>\n<p>\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]/g, (_, label) => `<blockquote class="callout"><p><strong>${label.charAt(0) + label.slice(1).toLowerCase()}.</strong>`);
  html = html.replace(/<table>/g, '<div class="table-scroll" tabindex="0" role="region" aria-label="Reference table"><table>').replace(/<\/table>/g, '</table></div>');
  return { html, headings };
}
export function renderCsv(text, delimiter = ',', limit = 100) {
  const rows = parse(text, { delimiter, bom: true, to: limit + 2, skip_empty_lines: true });
  if (!rows.length) throw new Error('CSV is empty');
  const header = rows.shift();
  const clipped = rows.length > limit;
  const body = rows.slice(0, limit);
  return `<p>${clipped ? `Preview limited to the first ${limit} rows. Download the file for all data.` : `${body.length} data rows. Values are displayed as stored.`}</p>
  <div class="data-controls" hidden><label>Filter rows <input type="search" data-table-filter placeholder="Type a value…"></label><span data-table-count role="status"></span></div>
  <div class="table-scroll" tabindex="0" role="region" aria-label="CSV data preview"><table data-csv><thead><tr>${header.map(h => `<th scope="col">${escape(h)}</th>`).join('')}</tr></thead><tbody>${body.map(row => `<tr>${row.map(cell => `<td>${escape(cell)}</td>`).join('')}</tr>`).join('')}</tbody></table></div>`;
}
export function previewTechnical(text, language, maxLines = 100) {
  let formatted = text;
  if (language === 'json') formatted = JSON.stringify(JSON.parse(text), null, 2);
  if (language === 'yaml') {
    const doc = parseDocument(text, { schema: 'core', uniqueKeys: true });
    if (doc.errors.length || doc.warnings.length) throw new Error('YAML contains unsupported tags or invalid content');
    formatted = doc.toString();
  }
  const lines = formatted.split('\n');
  const code = lines.slice(0, maxLines).join('\n');
  return `${lines.length > maxLines ? `<p>Preview: first ${maxLines} lines. Download the original for the complete file.</p>` : ''}<pre><code class="language-${escape(language)}">${hljs.getLanguage(language) ? hljs.highlight(code, { language }).value : escape(code)}</code></pre>`;
}

export function downloadPath(item) {
  const target = item.download || item.source;
  if (!target || target.startsWith('/') || target.includes('\\') || target.split('/').some(part => !part || part.startsWith('.'))) throw new Error(`Download needs a non-hidden, relative public path: ${target}`);
  return target;
}
