// Reproduce upload-pages-artifact@v4's Linux packaging exclusions before deploying.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'bruce-pages-package-'));
const archive = path.join(temporary, 'artifact.tar');
const unpacked = path.join(temporary, 'unpacked');
fs.mkdirSync(unpacked);
execFileSync('tar', ['--dereference', '--hard-dereference', '--directory', path.resolve('_site'), '-cf', archive, '--exclude=.git', '--exclude=.github', '--exclude=.[^/]*', '.']);
execFileSync('tar', ['-xf', archive, '-C', unpacked]);
execFileSync(process.execPath, ['scripts/check-site.mjs'], { stdio: 'inherit', env: { ...process.env, SITE_OUTPUT: unpacked } });
console.log('Pages archive verified; local inspection copy:', temporary);
