// Progressive enhancements. Document content and navigation are static HTML.
for (const pre of document.querySelectorAll('pre')) {
  pre.tabIndex = 0;
  const code = pre.querySelector('code');
  if (!code || !navigator.clipboard?.writeText) continue;
  const button = document.createElement('button');
  button.className = 'copy-button'; button.type = 'button'; button.textContent = 'Copy';
  button.setAttribute('aria-label', 'Copy code to clipboard');
  button.addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(code.textContent); button.textContent = 'Copied'; }
    catch { button.textContent = 'Select and copy'; }
    setTimeout(() => { button.textContent = 'Copy'; }, 2200);
  });
  pre.append(button);
}
const menu = document.querySelector('.docs-menu');
if (menu) {
  const mobile = matchMedia('(max-width: 560px)');
  const syncMenu = () => { menu.open = !mobile.matches; };
  syncMenu(); mobile.addEventListener('change', syncMenu);
}
for (const table of document.querySelectorAll('table[data-csv]')) {
  const controls = table.closest('.prose').querySelector('.data-controls');
  controls.hidden = false;
  const rows = [...table.tBodies[0].rows];
  const count = controls.querySelector('[data-table-count]');
  const input = controls.querySelector('input');
  function filter() {
    const value = input.value.trim().toLocaleLowerCase('en');
    rows.forEach(row => { row.hidden = ![...row.cells].some(cell => cell.textContent.toLocaleLowerCase('en').includes(value)); });
    count.textContent = `${rows.filter(row => !row.hidden).length} of ${rows.length} preview rows`;
  }
  input.addEventListener('input', filter); filter();
  const collator = new Intl.Collator('en', { numeric: true, sensitivity: 'base' });
  for (const [i, th] of [...table.tHead.rows[0].cells].entries()) {
    const button = document.createElement('button');
    button.type = 'button'; button.textContent = th.textContent;
    button.setAttribute('aria-label', `Sort by ${th.textContent}`);
    th.textContent = ''; th.append(button); th.setAttribute('aria-sort', 'none');
    button.addEventListener('click', () => {
      const direction = th.getAttribute('aria-sort') === 'ascending' ? -1 : 1;
      table.querySelectorAll('th').forEach(header => header.setAttribute('aria-sort', 'none'));
      th.setAttribute('aria-sort', direction === 1 ? 'ascending' : 'descending');
      const ordered = [...rows].sort((a, b) => direction * collator.compare(a.cells[i].textContent, b.cells[i].textContent));
      table.tBodies[0].append(...ordered);
    });
  }
}
const searchForm = document.querySelector('#search-form');
if (searchForm) {
  const input = document.querySelector('#search-input');
  const results = document.querySelector('#search-results');
  const status = document.querySelector('#search-status');
  let index;
  const render = () => {
    const query = input.value.trim().slice(0, 200);
    const terms = query.toLocaleLowerCase('en').split(/\s+/).filter(Boolean);
    results.replaceChildren();
    if (!terms.length) { status.textContent = 'Search by command, topic, or filename.'; return; }
    const matches = index.map(page => {
      const title = page.title.toLocaleLowerCase('en');
      const all = `${title} ${page.description} ${page.text}`.toLocaleLowerCase('en');
      return { page, score: terms.every(term => all.includes(term)) ? terms.reduce((sum, term) => sum + (title.includes(term) ? 10 : 1), 0) : 0 };
    }).filter(item => item.score > 0).sort((a, b) => b.score - a.score).slice(0, 30);
    status.textContent = matches.length ? `${matches.length} result${matches.length === 1 ? '' : 's'} for “${query}”` : `No results for “${query}”. Try a shorter command or browse the documentation.`;
    for (const { page } of matches) {
      const a = document.createElement('a'); a.className = 'search-result'; a.href = page.url;
      const group = document.createElement('small'); group.textContent = page.group;
      const title = document.createElement('h2'); title.textContent = page.title;
      const summary = document.createElement('p'); summary.textContent = page.description;
      a.append(group, title, summary); results.append(a);
    }
  };
  searchForm.addEventListener('submit', event => {
    event.preventDefault(); if (!index) return;
    const url = new URL(location.href);
    if (input.value.trim()) url.searchParams.set('q', input.value.trim()); else url.searchParams.delete('q');
    history.replaceState(null, '', url); render();
  });
  input.addEventListener('input', () => { if (index) render(); });
  fetch(`${document.body.dataset.base}search-index.json`).then(response => {
    if (!response.ok) throw new Error('Search index unavailable');
    return response.json();
  }).then(data => {
    index = data; input.disabled = false; searchForm.querySelector('button').disabled = false;
    input.value = new URL(location.href).searchParams.get('q') || ''; render();
  }).catch(() => { status.textContent = 'Search could not load. Please use the documentation index below or refresh the page.'; });
}
