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
    // A previous release may still serve an older benchmark schema while a new
    // release is being built. Keep the reviewed carried data until current data arrives.
    const html = fs.readFileSync(path.join(__dirname, '../../site/index.html'), 'utf8');
    const carried = JSON.parse(html.match(/<script type="application\/json" id="bench-data">([\s\S]*?)<\/script>/)[1]);
    const benchmarkScript = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].at(-1)[1];
    const stale = structuredClone(carried);
    stale.languages.historical = stale.languages.skrumi;
    delete stale.languages.skrumi;
    for (const data of [stale, {...carried, date: '2030-01-02'}]) {
      await page.setContent('<div id="bench-list"></div><div id="bench-langs"></div><div id="bench-versions"></div><div id="bench-legend"></div><div id="bench-meta"></div><div id="bench-versions-intro"></div><script type="application/json" id="bench-data">' + JSON.stringify(carried) + '</script>');
      await page.evaluate(data => {window.fetch = async () => ({ok: true, json: async () => data});}, data);
      await page.addScriptTag({content: benchmarkScript});
      await page.waitForFunction(() => document.querySelector('#bench-meta').textContent.length > 0);
      assert.equal(await page.locator('#bench-langs .is-skrumi').count(), carried.benchmarks.length);
      const shownDate = (data === stale ? carried.date : data.date).split('-').reverse().join('.');
      assert((await page.locator('#bench-meta').textContent()).includes(shownDate));
    }
    console.log('BENCHMARK_SCHEMA_TRANSITION_OK');
    console.log('DOWNLOAD_AVAILABILITY_OK');
  } finally {await browser.close();}
})().catch(error => {console.error(error); process.exit(1);});
