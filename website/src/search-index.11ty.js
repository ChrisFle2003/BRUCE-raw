import { getConfig } from '../lib/catalog.mjs';
export default class {
  data() { return { permalink: 'search-index.json' }; }
  render({ catalog }) { return JSON.stringify(catalog.map(p => ({ title: p.title, description: p.description, group: p.group, url: getConfig().base + p.url, text: p.source + " " + p.searchText }))); }
}
