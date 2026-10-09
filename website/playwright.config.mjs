import { defineConfig } from '@playwright/test';
import { getConfig } from './lib/catalog.mjs';
export default defineConfig({
  testDir: './test/browser', timeout: 30000, workers: 2,
  use: { baseURL: `http://127.0.0.1:8080${getConfig().base}`, headless: true },
  webServer: { command: 'node scripts/serve.mjs', url: `http://127.0.0.1:8080${getConfig().base}`, reuseExistingServer: !process.env.CI },
  reporter: 'list'
});
