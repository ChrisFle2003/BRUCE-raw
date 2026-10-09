import { docLayout } from '../lib/layout.mjs';
export default class {
  data() { return { pagination: { data: 'catalog', size: 1, alias: 'doc' }, permalink: data => data.doc.url + 'index.html' }; }
  render(data) { return docLayout(data.doc, data.catalog); }
}
