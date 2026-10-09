import test from 'node:test';
import assert from 'node:assert/strict';
import { configFor, renderMarkdown, renderCsv, previewTechnical, resolveLink } from '../lib/content.mjs';

test('deployment URLs support project, root, custom domain and reject unsafe paths', () => {
  assert.equal(configFor('https://chrisfle2003.github.io/My-Bruce-AI/').base, '/My-Bruce-AI/');
  assert.equal(configFor('https://example.org/').base, '/');
  assert.equal(configFor('https://example.org/research').base, '/research/');
  assert.throws(() => configFor('javascript:alert(1)'));
});
test('Markdown is inert, highlighted and headings have unique addressable IDs', () => {
  const { html, headings } = renderMarkdown('# Test\n## Same\n## Same\n<script>alert(1)</script>\n\n[x](javascript:alert(1))\n\n```json\n{"x":1}\n```', { source: 'README.md', pages: [], base: '/demo/' });
  assert.deepEqual(headings.map(h => h.id), ['test', 'same', 'same-1']);
  assert.ok(!html.includes('<script>'));
  assert.ok(!html.includes('href="javascript:'));
  assert.match(html, /hljs-number/);
});
test('relative selected links, source links and fragments retain the deployment prefix', () => {
  const pages = [{ source: 'docs/cli/formats.md', url: 'docs/formats/', headingIds: ['numbers'] }];
  const ctx = { source: 'docs/cli/README.md', pages, base: '/My-Bruce-AI/' };
  assert.equal(resolveLink('formats.md#numbers', ctx), '/My-Bruce-AI/docs/formats/#numbers');
  assert.match(resolveLink('../../apps/bruce_cli.cpp', ctx), /blob\/main-beta-001\/apps\/bruce_cli.cpp$/);
  assert.match(resolveLink('formats.md#untranslated-section', ctx), /blob\/main-beta-001\/docs\/cli\/formats.md#untranslated-section$/);
  assert.throws(() => resolveLink('../../../.env', ctx));
});
test('CSV preserves quoted delimiters, multiline Unicode, empty values and limits rows', () => {
  const csv = 'name;value;note\r\n"Müller; A";"1/2";"two\nlines"\r\n"<script>";;empty\r\n';
  const html = renderCsv(csv, ';', 1);
  assert.match(html, /Müller; A/);
  assert.match(html, /1\/2/);
  assert.match(html, /two\nlines/);
  assert.match(html, /first 1 rows/);
  assert.ok(!html.includes('<script>'));
  const full = renderCsv(csv, ';', 100);
  assert.match(full, /&lt;script&gt;/);
  assert.match(full, /<td><\/td>/);
});
test('technical previews format data but never execute it', () => {
  assert.match(previewTechnical('{"x":"<script>"}', 'json'), /&lt;script&gt;/);
  assert.throws(() => previewTechnical('x: !!js/function function() {}', 'yaml'));
});
test('public download paths cannot contain hidden directories excluded by Pages packaging', async () => {
  const { downloadPath } = await import('../lib/content.mjs');
  assert.equal(typeof downloadPath, 'function');
  assert.equal(downloadPath({ source: '.github/workflows/ci.yml', download: 'workflows/ci.yml' }), 'workflows/ci.yml');
  assert.throws(() => downloadPath({ source: '.github/workflows/ci.yml' }));
  assert.throws(() => downloadPath({ source: '../.env' }));
});
