import test from 'node:test';
import assert from 'node:assert/strict';
import { parseHTML } from 'linkedom';
import { getContactConfig } from '../lib/contact.mjs';
import { renderContact } from '../src/contact.11ty.js';

test('missing setup never renders an active submission destination', () => {
  const c = getContactConfig({ endpoint:'', serverChecksConfigured:false, privacyNoticeUrl:'' });
  assert.equal(c.available, false);
  const { document } = parseHTML(renderContact(c));
  assert.equal(document.querySelector('form'), null);
  assert.ok(document.querySelector('fieldset[disabled]'));
  assert.match(document.body.textContent, /not available yet/);
});
test('configuration accepts only public Formspree endpoints without credentials', () => {
  for (const endpoint of ['https://evil.example/f/abc', 'https://formspree.io@evil.example/f/abc', 'http://formspree.io/f/abc', 'https://formspree.io/f/abc?key=secret', 'https://formspree.io/f/abc#hash']) {
    assert.throws(() => getContactConfig({ endpoint, serverChecksConfigured:true }));
  }
  assert.equal(getContactConfig({ endpoint:'https://formspree.io/f/testonly', serverChecksConfigured:false }).available, false);
  assert.throws(() => getContactConfig({ endpoint:'', privacyNoticeUrl:'javascript:alert(1)' }));
});
test('configured form uses semantic required fields and a native POST fallback', () => {
  const c = getContactConfig({ endpoint:'https://formspree.io/f/testonly', serverChecksConfigured:true, privacyNoticeUrl:'https://example.org/privacy/' });
  const { document } = parseHTML(renderContact(c));
  assert.equal(document.querySelector('form').getAttribute('method'), 'post');
  assert.equal(document.querySelector('form').getAttribute('action'), c.endpoint);
  for (const field of ['name','email','message']) assert.ok(document.querySelector(`[name="${field}"][required]`));
  assert.equal(document.querySelector('[name="subject"]').hasAttribute('required'), false);
  assert.equal(document.querySelector('[name="email"]').getAttribute('autocomplete'), 'email');
  assert.equal(document.querySelector('[name="name"]').getAttribute('autocomplete'), 'name');
  assert.ok(document.querySelector('[role="status"][aria-live="polite"]'));
  assert.ok(document.querySelector('a[href="https://example.org/privacy/"]'));
});
