// Preview the production files with the real deployment prefix; no SPA fallback.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { getConfig } from '../lib/catalog.mjs';
const { base } = getConfig();
const root = path.resolve('_site');
const types = { '.html':'text/html; charset=utf-8', '.css':'text/css', '.js':'text/javascript', '.json':'application/json', '.svg':'image/svg+xml', '.png':'image/png', '.xml':'application/xml', '.csv':'text/csv; charset=utf-8' };
const server = http.createServer((req,res) => {
  let pathname;
  try { pathname = decodeURIComponent(new URL(req.url, 'http://localhost').pathname); } catch { res.writeHead(400); res.end(); return; }
  if (pathname === base.slice(0,-1) && base !== '/') { res.writeHead(301, { location: base }); res.end(); return; }
  const file = path.resolve(root, '.' + '/' + pathname.slice(base.length));
  if (!pathname.startsWith(base) || (file !== root && !file.startsWith(root + path.sep))) { res.writeHead(404); res.end('Not found'); return; }
  let target = file;
  if (fs.existsSync(target) && fs.statSync(target).isDirectory()) {
    if (!pathname.endsWith('/')) { res.writeHead(301, { location: pathname + '/' }); res.end(); return; }
    target = path.join(target, 'index.html');
  }
  if (!fs.existsSync(target) || !fs.statSync(target).isFile()) { res.writeHead(404, { 'Content-Type':types['.html'] }); res.end(fs.readFileSync(path.join(root,'404.html'))); return; }
  res.writeHead(200, { 'Content-Type': types[path.extname(target)] || 'text/plain; charset=utf-8' }); fs.createReadStream(target).pipe(res);
});
server.listen(Number(process.env.PORT || 8080), '127.0.0.1', () => console.log(`Preview: http://127.0.0.1:${process.env.PORT || 8080}${base}`));
