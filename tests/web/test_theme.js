// One palette and theme preference across all three sites, including mobile.
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');
const {chromium} = require('playwright');
(async () => {
  const root = path.resolve(__dirname, '../../site');
  const server = http.createServer(async (req, res) => {
    const url = new URL(req.url, 'http://localhost');
    const file = path.resolve(root, '.' + url.pathname, url.pathname.endsWith('/') ? 'index.html' : '');
    try {
      if (!file.startsWith(root + path.sep)) throw Error('path');
      res.setHeader('Content-Type', {'.html':'text/html', '.css':'text/css', '.js':'text/javascript', '.svg':'image/svg+xml', '.wasm':'application/wasm'}[path.extname(file)] || 'text/plain');
      res.end(await fs.readFile(file));
    } catch {res.writeHead(404); res.end();}
  });
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  const browser = await chromium.launch();
  try {
    for (const mobile of [false, true]) {
      const context = await browser.newContext({viewport: {width: mobile ? 390 : 1440, height: 900}, colorScheme: 'light'});
      const pages = [];
      const errors = [];
      for (const route of ['', 'playground/', 'builder/']) {
        const page = await context.newPage();
        page.on('pageerror', e => errors.push(e.message));
        await page.goto(`http://127.0.0.1:${server.address().port}/${route}`);
        pages.push(page);
      }
      const tokens = page => page.evaluate(() => {
        const s = getComputedStyle(document.documentElement);
        return ['--bg','--surface','--ink','--skrumi','--body','--display'].map(k => s.getPropertyValue(k).trim());
      });
      for (const p of pages) assert.deepEqual(await tokens(p), await tokens(pages[0]));
      // A click must visibly change theme even when starting in system mode.
      await pages[2].locator('#theme').click();
      for (const p of pages) {
        await p.waitForFunction(() => document.documentElement.dataset.theme === 'dark');
        assert.equal(await p.evaluate(() => getComputedStyle(document.body).fontFamily), await pages[0].evaluate(() => getComputedStyle(document.body).fontFamily));
        assert(await p.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), 'horizontal overflow');
      }
      for (let i = 0; i < pages.length; i++) {
        await pages[i].screenshot({path: `/tmp/skrumi-unified-${i}-${mobile ? 'mobile' : 'desktop'}.png`});
      }
      await pages[2].reload();
      assert.equal(await pages[2].locator('html').getAttribute('data-theme'), 'dark');
      await pages[1].locator('#theme').click();
      for (const p of pages) await p.waitForFunction(() => document.documentElement.dataset.theme === 'light');
      // Explicit choice overrides a change in the operating-system preference.
      await pages[2].emulateMedia({colorScheme: 'dark'});
      assert.equal(await pages[2].locator('html').getAttribute('data-theme'), 'light');
      await pages[2].locator('#theme').click({modifiers:['Shift']});
      await pages[2].waitForFunction(() => document.documentElement.dataset.theme === 'dark');
      await pages[2].emulateMedia({colorScheme: 'light'});
      await pages[2].waitForFunction(() => document.documentElement.dataset.theme === 'light');
      assert.deepEqual(errors, []);
      await context.close();
      console.log('SHARED_THEME_OK', mobile ? 'mobile' : 'desktop');
    }
    const context = await browser.newContext();
    await context.addInitScript(() => {
      Storage.prototype.getItem = Storage.prototype.setItem = () => {throw Error('Storage blocked');};
    });
    const page = await context.newPage();
    await page.goto(`http://127.0.0.1:${server.address().port}/builder/`);
    const before = await page.locator('html').getAttribute('data-theme');
    await page.locator('#theme').click();
    assert.notEqual(await page.locator('html').getAttribute('data-theme'), before);
    await context.close();
    console.log('THEME_WITHOUT_STORAGE_OK');
  } finally {await browser.close(); server.close();}
})().catch(e => {console.error(e); process.exitCode = 1;});
