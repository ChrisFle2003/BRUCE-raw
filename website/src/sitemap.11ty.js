import { getConfig } from '../lib/catalog.mjs';
import { escape } from '../lib/content.mjs';
export default class {
  data() { return { permalink: 'sitemap.xml' }; }
  render({ catalog }) { return `<?xml version="1.0" encoding="UTF-8"?><urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">${['', 'docs/', 'contact/', 'search/', ...catalog.map(p => p.url)].map(p => `<url><loc>${escape(getConfig().url + p)}</loc></url>`).join('')}</urlset>`; }
}
