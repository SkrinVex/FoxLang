const assert = require("node:assert/strict");
const fs = require("node:fs/promises");
const http = require("node:http");
const path = require("node:path");
const { chromium } = require("playwright");
const M = require("../../site/builder/model");
const runtime = process.argv.includes("--runtime");
(async () => {
  const root = path.resolve(__dirname, "../../site");
  const server = http.createServer(async (req, res) => {
    try {
      const url = new URL(req.url, "http://localhost"),
        file = path.resolve(
          root,
          "." + decodeURIComponent(url.pathname),
          url.pathname.endsWith("/") ? "index.html" : "",
        );
      if (!file.startsWith(root + path.sep)) throw Error("path");
      res.setHeader(
        "Content-Type",
        {
          ".html": "text/html; charset=utf-8",
          ".js": "application/javascript; charset=utf-8",
          ".css": "text/css",
          ".svg": "image/svg+xml",
          ".wasm": "application/wasm",
        }[path.extname(file)] || "text/plain",
      );
      res.end(await fs.readFile(file));
    } catch {
      res.writeHead(404);
      res.end();
    }
  });
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  let browser;
  try {
    browser = await chromium.launch(
      process.env.CHROMIUM_PATH
        ? { executablePath: process.env.CHROMIUM_PATH }
        : {},
    );
    for (const mobile of [false, true]) {
      const context = await browser.newContext({
        viewport: mobile
          ? { width: 390, height: 844 }
          : { width: 1440, height: 1080 },
        isMobile: mobile,
        hasTouch: mobile,
      });
      const errors = [];
      const page = await context.newPage();
      page.on("pageerror", (e) => errors.push(e.message));
      if (!runtime)
        await page.addInitScript(() => {
          window.workerRuns = [];
          window.Worker = class {
            constructor() {
              setTimeout(
                () => this.emit({ type: "ready", version: "test" }),
                0,
              );
            }
            emit(data) {
              this.onmessage?.({ data });
            }
            postMessage(m) {
              if (m.type === "check")
                setTimeout(
                  () => this.emit({ type: "problems", id: m.id, problems: {} }),
                  0,
                );
              if (m.type === "run") {
                window.workerRuns.push(m);
                if (m.canvas)
                  setTimeout(
                    () =>
                      this.emit({
                        type: "window",
                        width: 800,
                        height: 560,
                        title: "Generated",
                      }),
                    0,
                  );
              }
            }
            terminate() {}
          };
        });
      await page.goto(`http://127.0.0.1:${server.address().port}/builder/`);
      await page.locator("#stage .widget").first().waitFor();
      assert.equal(await page.locator("#stage .widget").count(), 5);
      const stageColor = await page.locator("#stage").evaluate(el => getComputedStyle(el).backgroundColor);
      await page.emulateMedia({ colorScheme: "dark" });
      await page.waitForFunction(() => document.documentElement.dataset.theme === "dark");
      assert.equal(await page.locator("html").getAttribute("data-theme-preference"), "auto");
      await page.locator("#theme").click();
      assert.equal(await page.locator("html").getAttribute("data-theme"), "light");
      await page.reload();
      await page.locator("#stage .widget").first().waitFor();
      assert.equal(await page.locator("html").getAttribute("data-theme-preference"), "light");
      assert.equal(await page.locator("html").getAttribute("data-theme"), "light");
      await page.locator("#theme").click();
      assert.equal(await page.locator("html").getAttribute("data-theme"), "dark");
      assert.equal(await page.locator("#stage").evaluate(el => getComputedStyle(el).backgroundColor), stageColor);
      assert(await page.locator(".brand img").evaluate(img => img.complete && img.naturalWidth > 0));
      await page.screenshot({path: `/tmp/skrumi-builder-dark-${mobile ? "mobile" : "desktop"}.png`});
      await page.locator("#theme").click({modifiers: ["Shift"]});
      await page.emulateMedia({ colorScheme: "light" });
      await page.waitForFunction(() => document.documentElement.dataset.theme === "light");

      await page.locator("[data-mode=desktop]").click();
      assert(
        await page
          .locator("#palette")
          .evaluate((p) =>
            [...p.children].every(
              (b) =>
                b.getBoundingClientRect().right <=
                p.getBoundingClientRect().right + 1,
            ),
          ),
        "palette buttons must not be clipped",
      );
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth + 1,
        ),
        "page must fit viewport",
      );
      await page.locator("[data-add=progress]").click();
      await page.locator("#progress").fill("65");
      await page.locator("#progress").press("Tab");
      assert.equal(await page.locator("#stage .progress").textContent(), "65%");
      await page.locator("#application-theme").selectOption("dark");
      assert((await page.locator("#stage .progress").getAttribute("style")).includes("linear-gradient"));
      await page.locator("#application-theme").selectOption("legacy");
      await page.locator("#delete").click();
      await page.locator("[data-add=button]").click();
      await page.locator("#text").fill('Нажать " 🦊');
      await page.locator("#text").press("Tab");
      assert.equal(
        await page.locator("#stage .button").textContent(),
        'Нажать " 🦊',
      );
      await page.locator("#x").fill("42");
      await page.locator("#x").press("Tab");
      await page.locator("#undo").click();
      assert.equal(await page.locator("#x").inputValue(), "24");
      await page.locator("#redo").click();
      assert.equal(await page.locator("#x").inputValue(), "42");
      await page.locator("#duplicate").click();
      assert.equal(await page.locator("#stage .widget").count(), 7);
      const deletedId = await page.locator("#element-id").textContent();
      await page.locator("#delete").click();
      assert.equal(await page.locator("#stage .widget").count(), 6);
      await page.locator("#undo").click();
      await page.locator("#redo").click();
      await page.locator("#layers button").last().click();
      await page.locator("#duplicate").click();
      assert.notEqual(
        await page.locator("#element-id").textContent(),
        deletedId,
        "deleted IDs are never reused, including after undo/redo",
      );
      await page.locator("#undo").click();
      if (!mobile) {
        const item = page.locator("#stage .button");
        const box = await item.boundingBox();
        await page.mouse.move(box.x + 10, box.y + 10);
        await page.mouse.down();
        await page.mouse.move(box.x + 30, box.y + 30, { steps: 4 });
        await page.mouse.up();
        assert(
          Number(await page.locator("#x").inputValue()) > 42,
          "drag updates coordinates",
        );
        const handle = page.locator(".selected .handle");
        const h = await handle.boundingBox();
        const before = Number(await page.locator("#w").inputValue());
        await page.mouse.move(h.x + h.width / 2, h.y + h.height / 2);
        await page.mouse.down();
        await page.mouse.move(h.x + 30, h.y + 20, { steps: 4 });
        await page.mouse.up();
        assert(
          Number(await page.locator("#w").inputValue()) > before,
          "resize updates dimensions",
        );
      }
      if (mobile) {
        const item = page.locator("#stage .button");
        await item.scrollIntoViewIfNeeded();
        const box = await item.boundingBox();
        const cdp = await context.newCDPSession(page),
          x = box.x + 10,
          y = box.y + 10;
        await cdp.send("Input.dispatchTouchEvent", {
          type: "touchStart",
          touchPoints: [{ x, y }],
        });
        await cdp.send("Input.dispatchTouchEvent", {
          type: "touchMove",
          touchPoints: [{ x: x + 15, y: y + 15 }],
        });
        await cdp.send("Input.dispatchTouchEvent", {
          type: "touchEnd",
          touchPoints: [],
        });
        assert(
          Number(await page.locator("#x").inputValue()) > 42,
          "touch drag updates coordinates",
        );
        await cdp.detach();
      }
      await page.locator("[data-mode=mobile]").click();
      await page.locator("#layers button").last().click();
      assert.equal(
        await page.locator("#x").inputValue(),
        "24",
        "mobile coordinates are independent",
      );
      await page.locator("#x").fill("16");
      await page.locator("#x").press("Tab");
      await page.locator("[data-tab=code]").click();
      // Edit user logic, then move the existing button and return to code.
      assert.equal(await page.locator("#code").getAttribute("readonly"), null);
      const customMain = (await page.locator("#code").inputValue()).replace(
        "print(layout_message(id));",
        'print("MY_HANDLER=" + id);',
      );
      await page.locator("#code").fill(customMain);
      await page.locator("[data-tab=design]").click();
      const moved = page.locator("#stage .button");
      await moved.evaluate((node) =>
        node.scrollIntoView({ block: "center", behavior: "instant" }),
      );
      await page.evaluate(
        () =>
          new Promise((resolve) =>
            requestAnimationFrame(() => requestAnimationFrame(resolve)),
          ),
      );
      const movedBox = await moved.boundingBox();
      await page.mouse.move(movedBox.x + 10, movedBox.y + 10);
      await page.mouse.down();
      await page.mouse.move(movedBox.x + 28, movedBox.y + 20, { steps: 4 });
      await page.mouse.up();
      assert(Number(await page.locator("#x").inputValue()) > 16);
      await page.locator("#undo").click();
      await page.locator("#redo").click();
      await page.locator("[data-tab=code]").click();
      assert.equal(await page.locator("#code").inputValue(), customMain);
      await page.locator("#code-file").selectOption("layout.skm");
      assert.notEqual(
        await page.locator("#code").getAttribute("readonly"),
        null,
      );
      assert(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
        "code controls fit the viewport",
      );
      const code = await page.locator("#code").inputValue();
      assert(code.includes("open_window(360, 640"));
      assert(code.includes("ui_button"));
      assert(code.includes('Нажать \\" 🦊'));
      const downloadPromise = page.waitForEvent("download");
      await page.locator("#download-layout").click();
      const download = await downloadPromise;
      assert.equal(await fs.readFile(await download.path(), "utf8"), code);
      const savePromise = page.waitForEvent("download");
      await page.locator("#save").click();
      const saved = JSON.parse(
        await fs.readFile(await (await savePromise).path(), "utf8"),
      );
      assert.equal(M.validate(saved).elements.length, 6);
      assert.equal(saved.main, customMain);
      const mainDownload = page.waitForEvent("download");
      await page.locator("#download").click();
      assert.equal(
        await fs.readFile(await (await mainDownload).path(), "utf8"),
        customMain,
      );
      const share = new URL(
        await page.locator("#playground").getAttribute("href"),
        page.url(),
      );
      const shared = JSON.parse(
        Buffer.from(share.hash.slice(7), "base64url").toString("utf8"),
      );
      assert.deepEqual(shared, { "main.skm": customMain, "layout.skm": code });
      await page.locator("#file").setInputFiles({
        name: "bad.json",
        mimeType: "application/json",
        buffer: Buffer.from('{"format":"no"}'),
      });
      await page.waitForFunction(() =>
        document
          .querySelector("#notice")
          .textContent.includes("Проект не изменён"),
      );
      assert.equal(await page.locator("#code").inputValue(), code);
      await page.reload();
      await page.locator("[data-tab=code]").click();
      assert.equal(await page.locator("#code").inputValue(), customMain);
      await page.locator("#code-file").selectOption("layout.skm");
      await page.locator("[data-mode=mobile]").click();
      assert.equal(
        await page.locator("#code").inputValue(),
        code,
        "autosave roundtrip",
      );
      await page.locator("#file").setInputFiles({
        name: "new.json",
        mimeType: "application/json",
        buffer: Buffer.from(
          JSON.stringify({ ...M.template(), main: customMain }),
        ),
      });
      await page.waitForFunction(
        () => document.querySelector("#count").textContent === "5",
      );
      // Preview is isolated from the visitor's existing playground program.
      const storageKey = "foxlang-playground-files";
      await page.evaluate(
        (k) => localStorage.setItem(k, "untouched"),
        storageKey,
      );
      await page.locator("[data-mode=desktop]").click();
      await page.locator("[data-tab=preview]").click();
      await page.waitForFunction(
        () => !document.querySelector("#run").disabled,
        {},
        { timeout: 60000 },
      );
      await page.locator("#run").click();
      const frame = page.frameLocator("#preview");
      await frame
        .locator("canvas")
        .waitFor({ state: "visible", timeout: 60000 });
      if (runtime) {
        const canvas = frame.locator("canvas");
        const box = await canvas.boundingBox();
        const s = box.width / 800;
        await page.mouse.click(box.x + 60 * s, box.y + 245 * s);
        await frame
          .locator("#output")
          .getByText("MY_HANDLER=", { exact: false })
          .waitFor({ timeout: 15000 });
      } else {
        const iframe = page
          .frames()
          .find((f) => f.url().includes("embed=builder"));
        const runs = await iframe.evaluate(() => window.workerRuns.length);
        assert(
          (
            await iframe.evaluate(
              () =>
                window.workerRuns
                  .at(-1)
                  .files.find((f) => f.name === "layout.skm").text,
            )
          ).includes("using ui;"),
        );
        assert.equal(
          await iframe.evaluate(
            () =>
              window.workerRuns.at(-1).files.find((f) => f.name === "main.skm")
                .text,
          ),
          customMain,
        );
        await iframe.evaluate(() =>
          window.dispatchEvent(
            new MessageEvent("message", {
              source: parent,
              origin: "https://other.invalid",
              data: {
                type: "foxlang-builder-run",
                files: { "main.skm": "print(123);", "layout.skm": "" },
              },
            }),
          ),
        );
        assert.equal(
          await iframe.evaluate(() => window.workerRuns.length),
          runs,
          "foreign messages cannot run programs",
        );
      }
      await page.locator("#stop").click();
      assert.equal(
        await page.evaluate((k) => localStorage.getItem(k), storageKey),
        "untouched",
      );
      assert.deepEqual(errors, []);
      if (process.env.BUILDER_SCREENSHOTS) {
        await page.locator("[data-tab=design]").click();
        if (mobile) await page.locator("[data-mode=mobile]").click();
        await page.screenshot({
          path: path.join(
            process.env.BUILDER_SCREENSHOTS,
            `builder-${mobile ? "mobile" : "desktop"}.png`,
          ),
          fullPage: true,
        });
      }
      await context.close();
      console.log(
        `BUILDER_BROWSER_OK ${mobile ? "mobile" : "desktop"} ${runtime ? "real runtime" : "protocol"}`,
      );
    }
  } finally {
    if (browser) await browser.close();
    await new Promise((r) => server.close(r));
  }
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
