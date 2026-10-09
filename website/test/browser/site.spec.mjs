import { test, expect } from '@playwright/test';
import { getConfig } from '../../lib/catalog.mjs';
const { base } = getConfig();
test('direct nested pages, search, code copy and exact downloads', async ({ page, context, request }) => {
  const failures = [];
  page.on('pageerror', e => failures.push(e.message));
  page.on('response', r => { if (r.status() >= 400) failures.push(r.url()); });
  await page.goto('docs/commands/');
  await page.reload();
  await expect(page.getByRole('heading', { name: 'Commands at a glance', exact: false })).toBeVisible();
  await context.grantPermissions(['clipboard-read', 'clipboard-write']);
  const copy = page.getByRole('button', { name: 'Copy code to clipboard' }).first();
  await copy.focus(); await page.keyboard.press('Enter');
  await expect(copy).toHaveText('Copied');
  expect(await page.evaluate(() => navigator.clipboard.readText())).toContain('--train examples/affine-training.csv');
  await page.goto('search/?q=replay');
  await expect(page.locator('#search-status')).toContainText('results for');
  await expect(page.locator('.search-result')).not.toHaveCount(0);
  const field = page.getByRole('searchbox');
  await field.fill('zzzzzz-no-result'); await page.keyboard.press('Enter');
  await expect(page.locator('#search-status')).toContainText('No results');
  await field.fill('rational');
  await page.keyboard.press('Tab'); await page.keyboard.press('Enter');
  await expect(page).toHaveURL(/q=rational/);
  const result = page.locator('.search-result').first();
  await result.focus(); await page.keyboard.press('Enter');
  await expect(page.locator('main')).toBeVisible();
  const download = await request.get(`${base}downloads/examples/affine-training.csv`);
  expect(await download.text()).toBe('x,y\n0,3\n1,5\n4,11\n');
  expect(failures).toEqual([]);
});
test('CSV sorting and filtering preserve source strings and are keyboard accessible', async ({ page }) => {
  await page.goto('docs/data/compatibility-candidates/');
  const sort = page.getByRole('button', { name: 'Sort by value', exact: true });
  await sort.focus(); await page.keyboard.press('Enter');
  await expect(page.locator('tbody tr').first().locator('td').last()).toHaveText('2');
  await page.keyboard.press('Enter');
  await expect(page.locator('tbody tr').first().locator('td').last()).toHaveText('102');
  await page.getByRole('searchbox', { name: 'Filter rows' }).fill('102');
  await expect(page.locator('tbody tr:visible')).toHaveCount(1);
  await expect(page.locator('[data-table-count]')).toHaveText('1 of 5 preview rows');
});
for (const width of [360, 768, 1440]) {
  test(`responsive navigation and layout at ${width}px`, async ({ page }) => {
    await page.setViewportSize({ width, height: 900 });
    for (const route of ['', 'docs/', 'docs/commands/', 'docs/data/compatibility-candidates/', 'search/']) {
      await page.goto(route || './');
      expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBeTruthy();
    }
    await page.goto('docs/commands/');
    if (width === 360) {
      const toggle = page.locator('.docs-menu summary');
      await toggle.focus(); await page.keyboard.press('Enter');
      await expect(page.locator('.docs-menu')).toHaveAttribute('open', '');
      await expect(page.getByRole('navigation', { name:'Documentation', exact:true }).getByRole('link', { name:'Input formats' })).toBeVisible();
    }
    await page.screenshot({ path: `test-results/docs-${width}.png`, fullPage: true });
    await page.goto('./');
    await page.screenshot({ path: `test-results/home-${width}.png`, fullPage: true });
  });
}
test('core navigation, headings and CSV content work without JavaScript', async ({ browser }) => {
  const context = await browser.newContext({ javaScriptEnabled: false });
  const page = await context.newPage();
  await page.goto(`http://127.0.0.1:8080${base}docs/data/affine-training/`);
  await expect(page.locator('tbody tr')).toHaveCount(3);
  await page.getByRole('navigation', { name:'Documentation', exact:true }).getByRole('link', { name:'Input formats' }).click();
  await expect(page.getByRole('heading', { name:'Research CSV pairs', exact:false })).toBeVisible();
  await page.goto(`http://127.0.0.1:8080${base}unknown/path`);
  await expect(page.getByRole('heading', { name:'No page at these coordinates.' })).toBeVisible();
  await context.close();
});
test('reduced motion and skip link', async ({ page }) => {
  await page.emulateMedia({ reducedMotion:'reduce' });
  await page.goto('./');
  await page.keyboard.press('Tab');
  await expect(page.getByRole('link', { name:'Skip to content' })).toBeFocused();
  await page.keyboard.press('Enter');
  await expect(page).toHaveURL(/#main$/);
  expect(await page.evaluate(() => getComputedStyle(document.documentElement).scrollBehavior)).toBe('auto');
});
test('documentation menu remains available when resizing from mobile to desktop', async ({ page }) => {
  await page.setViewportSize({ width:360, height:900 });
  await page.goto('docs/commands/');
  await expect(page.locator('.docs-menu')).not.toHaveAttribute('open');
  await page.setViewportSize({ width:1440, height:900 });
  await expect(page.getByRole('navigation', { name:'Documentation', exact:true }).getByRole('link', { name:'Input formats' })).toBeVisible();
});
