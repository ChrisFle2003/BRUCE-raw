import fs from 'node:fs';
import { downloadPath } from './lib/content.mjs';
import path from 'node:path';
import { manifest, repoRoot, getConfig } from './lib/catalog.mjs';
export default function(config) {
  config.addPassthroughCopy({ 'src/assets': 'assets' });
  // Only these audited inputs become downloads. Never copy the repository wholesale.
  for (const item of manifest) config.addPassthroughCopy({ [path.join(repoRoot, item.source)]: `downloads/${downloadPath(item)}` });
  config.addWatchTarget('content');
  config.addWatchTarget('contact.config.json');
  for (const item of manifest) config.addWatchTarget(path.join(repoRoot, item.source));
  config.addWatchTarget(path.join(repoRoot, 'CMakeLists.txt'));
  config.on('eleventy.after', () => {
    fs.writeFileSync('_site/.nojekyll', '');
    const { cname } = getConfig();
    if (cname) fs.writeFileSync('_site/CNAME', cname + '\n');
  });
  return { dir: { input: 'src', output: '_site' }, templateFormats: ['11ty.js'], htmlTemplateEngine: false };
}
