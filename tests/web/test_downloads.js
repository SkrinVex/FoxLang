const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {chromium} = require('playwright');
(async () => {
  const browser = await chromium.launch();
  try {
    const page = await browser.newPage();
    const script = fs.readFileSync(path.join(__dirname, '../../site/assets/downloads.js'), 'utf8');
    const assets = ['Skrumi-windows-x86_64.exe', 'Skrumi-linux-x86_64.run'].map(name => ({
      name, state: 'uploaded', size: 100, browser_download_url: `https://github.com/SkrinVex/Skrumi/releases/download/vNEXT/${name}`
    }));
    for (const [body, visible, error] of [
      [{tag_name:'vNEXT', assets}, 2, false],
      [{tag_name:'vOLD', assets:[]}, 0, false],
      [{tag_name:'vNEXT', assets:assets.slice(0,1)}, 1, false],
      [{tag_name:'vNEXT', assets:[{...assets[0], browser_download_url:'https://example.com/file'}]}, 0, false],
      [{}, 0, true],
    ]) {
      await page.route('https://api.github.com/**', route => route.fulfill({status:error ? 503 : 200, contentType:'application/json', body:JSON.stringify(body)}));
      await page.setContent('<p id="download-status"></p><a id="download-windows" hidden></a><a id="download-linux" hidden></a>');
      await page.addScriptTag({content:script});
      await page.waitForFunction(() => document.querySelector('#download-status').textContent.length > 0);
      assert.equal(await page.locator('a:not([hidden])').count(), visible);
      for (const href of await page.locator('a:not([hidden])').evaluateAll(nodes => nodes.map(n => n.href)))
        assert(href.startsWith('https://github.com/SkrinVex/Skrumi/releases/download/vNEXT/'));
      await page.unroute('https://api.github.com/**');
    }
    console.log('DOWNLOAD_AVAILABILITY_OK');
  } finally {await browser.close();}
})().catch(error => {console.error(error); process.exit(1);});
