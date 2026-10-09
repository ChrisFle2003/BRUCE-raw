import { test, expect } from '@playwright/test';
import { renderContact } from '../../src/contact.11ty.js';
import { getContactConfig } from '../../lib/contact.mjs';
import { getConfig } from '../../lib/catalog.mjs';
const { base } = getConfig();
// Synthetic endpoint exists only in intercepted test pages, never the production build.
const endpoint = 'https://formspree.io/f/testonly';
const configured = getContactConfig({ endpoint, serverChecksConfigured:true, privacyNoticeUrl:'' });
async function ready(page) {
  await page.route('**/contact/', route => route.fulfill({ contentType:'text/html', body:renderContact(configured) }));
  await page.goto('contact/');
}
async function fill(page) {
  await page.getByLabel('Name (required)', { exact:true }).fill('Test Visitor');
  await page.getByLabel('Email address (required)').fill('visitor@example.com');
  await page.getByLabel('Message (required)', { exact:true }).fill('A mocked contact test. Do not send.');
}
test.beforeEach(async ({ context }) => {
  // Fail closed: no test can reach a real submission service.
  await context.route('https://formspree.io/**', route => route.abort());
});
test('production contact page follows configuration and is linked from the footer', async ({ page }) => {
  const production = getContactConfig();
  await page.goto('./');
  await page.getByRole('navigation', { name:'Footer' }).getByRole('link', { name:'Contact', exact:true }).click();
  await expect(page).toHaveURL(new RegExp(`${base}contact/$`));
  await expect(page.getByRole('heading', { name:'Send a message' })).toBeVisible();
  await expect(page.locator('form')).toHaveCount(production.available ? 1 : 0);
  await expect(page.locator('.privacy-gap')).toHaveCount(production.privacyNoticeUrl ? 0 : 1);
  if (production.available) {
    await expect(page.locator('form')).toHaveAttribute('action', production.endpoint);
    await expect(page.getByRole('button', { name:'Send message' })).toBeEnabled();
  } else {
    await expect(page.getByRole('button', { name:'Send message' })).toBeDisabled();
  }
});
test('missing configuration renders an honest unavailable state', async ({ page }) => {
  const unavailable = getContactConfig({ endpoint:'', serverChecksConfigured:false, privacyNoticeUrl:'' });
  await page.route('**/contact/', route => route.fulfill({ contentType:'text/html', body:renderContact(unavailable) }));
  await page.goto('contact/');
  await expect(page.getByText('Contact is not available yet.')).toBeVisible();
  await expect(page.getByRole('button', { name:'Send message' })).toBeDisabled();
  await expect(page.locator('form')).toHaveCount(0);
  await expect(page.locator('.privacy-gap')).toBeVisible();
});
test('required and email validation focus the first invalid field without sending', async ({ page }) => {
  let posts=0;
  await page.route(endpoint, route => { posts++; return route.abort(); });
  await ready(page);
  await page.getByRole('button', { name:'Send message' }).click();
  await expect(page.locator('#contact-name')).toBeFocused();
  await expect(page.locator('#contact-name')).toHaveAttribute('aria-invalid','true');
  await expect(page.locator('#contact-status')).toContainText('Check');
  await fill(page);
  await page.locator('#contact-name').fill('   ');
  await page.locator('#contact-email').fill('not-an-email');
  await page.locator('#contact-message').fill('   ');
  await page.getByRole('button', { name:'Send message' }).click();
  await expect(page.locator('#contact-email-error')).toContainText('valid email');
  await expect(page.locator('#contact-message-error')).toContainText('Enter');
  expect(posts).toBe(0);
});
test('pending prevents duplicates and confirmed acceptance shows success', async ({ page }) => {
  let release;
  const gate=new Promise(resolve => { release=resolve; });
  let posts=0, payload;
  await page.route(endpoint, async route => { posts++; payload=route.request().postData(); await gate; await route.fulfill({ status:200, contentType:'application/json', body:'{"ok":true}' }); });
  await ready(page); await fill(page);
  await page.getByRole('button', { name:'Send message' }).focus(); await page.keyboard.press('Enter');
  await expect(page.locator('#contact-status')).toHaveText('Sending your message…');
  await expect(page.locator('button[type="submit"]')).toBeDisabled();
  await expect.poll(() => posts).toBe(1);
  await page.locator('form').evaluate(form => form.requestSubmit());
  expect(posts).toBe(1); release();
  await expect(page.locator('#contact-status')).toHaveText('Thank you. Formspree accepted your message.');
  await expect(page.locator('#contact-status')).toBeFocused();
  expect(payload).toContain('visitor@example.com');
  expect(await page.evaluate(() => [localStorage.length,sessionStorage.length])).toEqual([0,0]);
});
for (const scenario of [
  { name:'server validation', status:422, body:'{"errors":[{"field":"email","code":"TYPE_EMAIL","message":"<script>bad</script>"}]}', expected:'Check' },
  { name:'rate limit', status:429, body:'{}', expected:'wait' },
  { name:'service failure', status:503, body:'{}', expected:'could not confirm' },
  { name:'unconfirmed 200', status:200, body:'{}', expected:'could not confirm' },
  { name:'conflicting response', status:200, body:'{"ok":true,"errors":[{"code":"BLOCKED"}]}', expected:'could not confirm' },
  { name:'invalid JSON', status:200, body:'not JSON', expected:'could not confirm' },
  { name:'network error', abort:true, expected:'could not confirm' }
]) test(`${scenario.name} preserves values and announces failure`, async ({ page }) => {
  await page.route(endpoint, route => scenario.abort ? route.abort('failed') : route.fulfill({ status:scenario.status, contentType:'application/json', body:scenario.body }));
  await ready(page); await fill(page);
  await page.getByLabel('Subject (optional)').fill('Test subject');
  await page.getByRole('button', { name:'Send message' }).click();
  await expect(page.locator('#contact-status')).toContainText(scenario.expected);
  await expect(page.locator('#contact-name')).toHaveValue('Test Visitor');
  await expect(page.locator('#contact-email')).toHaveValue('visitor@example.com');
  await expect(page.locator('#contact-subject')).toHaveValue('Test subject');
  await expect(page.locator('#contact-message')).toHaveValue('A mocked contact test. Do not send.');
  await expect(page.getByRole('button', { name:'Send message' })).toBeEnabled();
  await expect(page.locator('#contact-status')).toHaveAttribute('aria-live','polite');
  expect(await page.evaluate(() => [localStorage.length,sessionStorage.length])).toEqual([0,0]);
});
test('timeout preserves the draft without an automatic retry', async ({ page }) => {
  let requests=0;
  await page.route(endpoint, () => { requests++; });
  await ready(page); await fill(page);
  await page.clock.install();
  await page.getByRole('button', { name:'Send message' }).click();
  await expect.poll(() => requests).toBe(1);
  await page.clock.fastForward(26000);
  await expect(page.locator('#contact-status')).toContainText('could not confirm');
  await expect(page.locator('#contact-message')).not.toHaveValue('');
  expect(requests).toBe(1);
});
test('native no-JavaScript POST keeps the draft in the original tab', async ({ browser }) => {
  const context=await browser.newContext({ javaScriptEnabled:false });
  await context.route('https://formspree.io/**', route => route.fulfill({ status:422, contentType:'text/html', body:'<h1>Mock rejection</h1>' }));
  await context.route('**/contact/', route => route.fulfill({ contentType:'text/html', body:renderContact(configured) }));
  const page=await context.newPage();
  await page.goto(`http://127.0.0.1:8080${base}contact/`); await fill(page);
  const popupEvent=context.waitForEvent('page');
  await page.getByRole('button', { name:'Send message' }).click();
  const popup=await popupEvent;
  await expect(popup.getByRole('heading', { name:'Mock rejection' })).toBeVisible();
  await expect(page.locator('#contact-message')).toHaveValue('A mocked contact test. Do not send.');
  await context.close();
});
for (const width of [360,1440]) test(`contact layout and keyboard navigation at ${width}px`, async ({ page }) => {
  await page.setViewportSize({ width,height:1000 });
  await page.emulateMedia({ reducedMotion:'reduce' });
  await ready(page);
  await page.locator('#contact-name').focus();
  for (const id of ['contact-email','contact-subject','contact-message']) {
    await page.keyboard.press('Tab'); await expect(page.locator(`#${id}`)).toBeFocused();
  }
  await page.keyboard.press('Tab'); await expect(page.getByRole('button',{name:'Send message'})).toBeFocused();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBeTruthy();
  await page.screenshot({path:`test-results/contact-${width}.png`,fullPage:true});
});
