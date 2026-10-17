/* Browser editor. The model is the single source for layout and generated code. */
(() => {
  "use strict";
  const M = window.SkrumiBuilder,
    $ = (id) => document.getElementById(id),
    key = "skrumi-ui-project-v1";
  let project = M.template(),
    mode = matchMedia("(max-width:600px)").matches ? "mobile" : "desktop",
    selected = null,
    tab = "design",
    undo = [],
    redo = [],
    scale = 1,
    frameReady = false,
    frameStarted = false;
  let noticeTimer,
    drag = null;
  try {
    const saved = localStorage.getItem(key);
    if (saved) project = M.validate(JSON.parse(saved));
  } catch (error) {
    notice("Сохранённый проект не удалось открыть: " + error.message);
  }
  function notice(message) {
    $("notice").textContent = message;
    clearTimeout(noticeTimer);
    noticeTimer = setTimeout(() => ($("notice").textContent = ""), 7000);
  }
  function persist() {
    try {
      localStorage.setItem(key, JSON.stringify(project));
      $("saved").textContent = "Сохранено в этом браузере";
    } catch {
      $("saved").textContent = "Автосохранение недоступно — скачайте проект";
    }
  }
  function change(fn) {
    const before = M.clone(project);
    try {
      fn();
      project = M.validate(project);
    } catch (error) {
      project = before;
      render();
      notice(error.message);
      return;
    }
    if (JSON.stringify(before) !== JSON.stringify(project)) {
      undo.push(before);
      if (undo.length > 50) undo.shift();
      redo = [];
      persist();
    }
    render();
  }
  function element() {
    return project.elements.find((e) => e.id === selected);
  }
  function pick(id) {
    selected = id;
    render();
  }
  const styleLabels = {text:"Текст", muted:"Вторичный текст", border:"Граница", field:"Поле", button:"Кнопка", buttonHover:"Кнопка при наведении", panel:"Панель", track:"Полоса прогресса"};
  function palette() {
    const dark = project.theme === "dark";
    return {text:dark?"#eef1f6":"#1e232a", muted:dark?"#a6aebc":"#78808a", border:dark?"#4b5362":"#c8ced8",
      field:dark?"#1e232c":"#ffffff", button:dark?"#303744":"#eef1f6", buttonHover:dark?"#404958":"#dee5ef",
      panel:dark?"#181d25":"#ffffff", track:dark?"#303744":"#eceff3", ...project.styles};
  }
  for (const [name, title] of Object.entries(styleLabels)) {
    const row = document.createElement("label");
    row.className = "check";
    const enable = document.createElement("input"), value = document.createElement("input");
    enable.type = "checkbox"; enable.id = "style-enabled-" + name;
    value.type = "color"; value.id = "style-" + name; value.setAttribute("aria-label", title);
    row.append(enable, document.createTextNode(title), value); $("styles-host").append(row);
    enable.onchange = () => change(() => { if (enable.checked) project.styles[name] = value.value; else delete project.styles[name]; });
    value.onchange = () => change(() => { project.styles[name] = value.value; });
  }
  function render() {
    const s = project.sizes[mode],
      e = element();
    document
      .querySelectorAll("[data-mode]")
      .forEach((b) =>
        b.setAttribute("aria-pressed", String(b.dataset.mode === mode)),
      );
    document
      .querySelectorAll("[data-tab]")
      .forEach((b) =>
        b.setAttribute("aria-pressed", String(b.dataset.tab === tab)),
      );
    for (const t of ["design", "preview", "code"])
      $(t + "-view").hidden = t !== tab;
    $("zoom-label").hidden = tab !== "design";
    $("properties-jump").hidden = tab !== "design";
    $("properties-jump").disabled = !e;
    $("undo").disabled = !undo.length;
    $("redo").disabled = !redo.length;
    $("size-label").textContent =
      `${mode === "desktop" ? "ПК" : "Телефон"} / ${s.width} × ${s.height}`;
    for (const id of ["title", "breakpoint", "background", "accent"])
      $(id).value = project[id];
    $("application-theme").value = project.theme;
    $("closeAction").value = project.closeAction;
    const colors = palette();
    for (const name of Object.keys(styleLabels)) {
      $("style-enabled-" + name).checked = Object.hasOwn(project.styles, name);
      $("style-" + name).value = colors[name];
      $("style-" + name).disabled = !Object.hasOwn(project.styles, name);
    }
    $("width").value = s.width;
    $("height").value = s.height;
    $("count").textContent = project.elements.length;
    $("layers").replaceChildren();
    for (const item of project.elements) {
      const li = document.createElement("li"),
        b = document.createElement("button");
      b.textContent = M.types[item.type] + " · " + (item.text || item.id);
      b.dataset.id = item.id;
      b.setAttribute("aria-pressed", String(item.id === selected));
      b.onclick = () => pick(item.id);
      li.append(b);
      $("layers").append(li);
    }
    $("empty").hidden = !!e;
    $("element-form").hidden = !e;
    if (e) {
      $("element-name").textContent = M.types[e.type];
      $("element-id").textContent = e.id;
      for (const id of ["text", "value", "message", "color", "scale"])
        $(id).value = e[id];
      $("checked").checked = e.checked;
      $("kind").value = e.kind;
      $("kind-label").hidden = e.type !== "dialog";
      for (const id of ["minimum", "maximum", "progress"]) $(id).value = e[id];
      $("percent").checked = e.percent;
      $("progress-properties").hidden = e.type !== "progress";
      for (const id of ["x", "y", "w", "h", "horizontal", "vertical"])
        $(id).value = e.layouts[mode][id];
      $("value-label").hidden = e.type !== "field";
      $("checked-label").hidden = !["checkbox", "dialog"].includes(e.type);
      $("message-label").hidden = !["button", "primary", "dialog"].includes(e.type);
      $("color-label").hidden = !["label", "panel"].includes(e.type);
      $("scale-label").hidden = e.type !== "label";
      $("h").disabled = e.type === "label";
      $("w").disabled = ["label", "checkbox"].includes(e.type);
      $("horizontal").querySelector("[value=stretch]").disabled = [
        "label",
        "checkbox",
      ].includes(e.type);
      $("backward").disabled = project.elements.indexOf(e) === 0;
      $("forward").disabled =
        project.elements.indexOf(e) === project.elements.length - 1;
    }
    draw();
    const generated = M.files(project, mode);
    const files = {"main.skm": project.main, "layout.skm": generated["layout.skm"] || generated["layout.skm"]};
    const code = files[$("code-file").value];
    $("code").readOnly = $("code-file").value !== "main.skm";
    if ($("code").value !== code) $("code").value = code;
    const bytes = new TextEncoder().encode(JSON.stringify(files));
    let binary = "";
    for (const byte of bytes) binary += String.fromCharCode(byte);
    $("playground").href =
      "../playground/#code=T" +
      btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
    if (frameStarted)
      $("preview-note").textContent =
        "Изменения применятся после нового запуска.";
  }
  function draw() {
    const s = project.sizes[mode];
    const available = Math.max(
      160,
      $("stage-scroll").clientWidth - (innerWidth <= 600 ? 24 : 44),
    );
    scale =
      $("zoom").value === "fit"
        ? Math.min(1, available / s.width)
        : Number($("zoom").value);
    $("stage-space").style.width = s.width * scale + "px";
    $("stage-space").style.height = s.height * scale + "px";
    Object.assign($("stage").style, {
      width: s.width + "px",
      height: s.height + "px",
      transform: `scale(${scale})`,
      background: project.background,
    });
    $("stage").replaceChildren();
    for (const e of [...project.elements.filter(e => e.type !== "dialog"), ...project.elements.filter(e => e.type === "dialog")]) {
      const r = M.rect(project, e, mode),
        node = document.createElement("div");
      node.className = `widget ${e.type}${e.id === selected ? " selected" : ""}${e.checked ? " on" : ""}`;
      node.dataset.id = e.id;
      node.setAttribute("role", "button");
      node.tabIndex = 0;
      node.setAttribute("aria-label", M.types[e.type] + ": " + e.text);
      node.setAttribute("aria-pressed", String(e.id === selected));
      Object.assign(node.style, {
        left: r.x + "px",
        top: r.y + "px",
        width: r.w + "px",
        height: r.h + "px",
      });
      node.style.setProperty("--accent", project.accent);
      if (e.type === "panel") node.style.background = e.color;
      else node.textContent = e.type === "field" ? e.value || e.text : e.text;
      if (e.type === "label") {
        node.style.color = e.color;
        node.style.fontSize = e.scale * 8 + "px";
      }
      if (e.type === "primary") node.style.background = project.accent;
      if (e.type === "button") node.style.background = "#eef1f6";
      const dark = project.theme === "dark";
      node.style.setProperty("--widget-field", dark ? "#1e232c" : "#ffffff");
      node.style.setProperty("--widget-border", dark ? "#4b5362" : "#c8ced8");
      if (e.type === "field") {
        node.style.background = dark ? "#1e232c" : "#ffffff";
        node.style.borderColor = dark ? "#4b5362" : "#c8ced8";
        node.style.color = e.value ? (dark ? "#eef1f6" : "#1e232a") : (dark ? "#a6aebc" : "#78808a");
      }
      if (e.type === "checkbox") {
        node.style.color = dark ? "#eef1f6" : "#1e232a";
        node.style.setProperty("--checkbox-side", e.layouts[mode].h + "px");
        node.style.gap = Math.max(3, Math.floor(e.layouts[mode].h * 6 / 14)) + "px";
      }
      if (e.type === "button") { node.style.background = dark ? "#303744" : "#eef1f6"; node.style.color = dark ? "#eef1f6" : "#1e232a"; }
      if (e.type === "progress") {
        const fraction = Math.max(0, Math.min(1, (e.progress - e.minimum) / (e.maximum - e.minimum)));
        node.textContent = e.percent ? Math.floor(fraction * 100) + "%" : "";
        node.style.color = dark ? "#eef1f6" : "#1e232a";
        node.style.border = `1px solid ${dark ? "#4b5362" : "#c8ced8"}`;
        node.style.background = `linear-gradient(to right, ${project.accent} ${fraction * 100}%, ${dark ? "#303744" : "#eceff3"} ${fraction * 100}%)`;
        node.style.display = "grid";
        node.style.placeItems = "center";
      }
      const colors = palette();
      node.style.setProperty("--widget-field", colors.field);
      node.style.setProperty("--widget-border", colors.border);
      if (["button", "field", "progress", "dialog"].includes(e.type)) node.style.borderColor = colors.border;
      if (["button", "checkbox", "progress", "dialog"].includes(e.type)) node.style.color = colors.text;
      if (e.type === "button") node.style.background = colors.button;
      if (e.type === "field") { node.style.background = colors.field; node.style.color = e.value ? colors.text : colors.muted; }
      if (e.type === "progress") {
        const percent = Math.max(0, Math.min(100, 100 * (e.progress - e.minimum) / (e.maximum - e.minimum)));
        node.style.background = `linear-gradient(to right, ${project.accent} ${percent}%, ${colors.track} ${percent}%)`;
      }
      if (e.type === "dialog") {
        node.textContent = ""; node.style.background = colors.panel; node.style.border = `1px solid ${colors.border}`;
        const heading = document.createElement("div"), body = document.createElement("div"), buttons = document.createElement("div");
        heading.textContent = e.text; heading.style.cssText = `height:32px;background:${project.accent};color:white;padding:10px 12px;box-sizing:border-box`;
        body.textContent = e.message; body.style.cssText = `padding:12px;white-space:pre-wrap;color:${e.kind === "error" ? "#dc4141" : colors.text}`;
        buttons.textContent = e.kind === "confirm" ? "Да          Нет          Отмена" : "OK";
        buttons.style.cssText = `position:absolute;bottom:14px;left:12px;right:12px;padding:8px;background:${colors.button};text-align:center`;
        node.append(heading, body, buttons);
        node.dataset.open = String(e.checked);
      }
      if (selected === e.id && !["label", "checkbox"].includes(e.type)) {
        const handle = document.createElement("span");
        handle.className = "handle";
        handle.setAttribute("aria-hidden", "true");
        node.append(handle);
      }
      node.addEventListener("keydown", (event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          pick(e.id);
        }
      });
      $("stage").append(node);
    }
  }
  for (const [type, name] of Object.entries(M.types)) {
    const b = document.createElement("button"),
      icon = document.createElement("b");
    icon.textContent = {
      label: "T",
      button: "▭",
      primary: "▶",
      field: "I",
      checkbox: "☑",
      panel: "▧",
      progress: "▰",
    }[type];
    icon.setAttribute("aria-hidden", "true");
    b.append(icon, document.createTextNode(name));
    b.dataset.add = type;
    b.onclick = () => change(() => (selected = M.add(project, type).id));
    $("palette").append(b);
  }
  document.querySelectorAll("[data-mode]").forEach(
    (b) =>
      (b.onclick = () => {
        mode = b.dataset.mode;
        render();
      }),
  );
  document.querySelectorAll("[data-tab]").forEach(
    (b) =>
      (b.onclick = () => {
        tab = b.dataset.tab;
        render();
        if (tab === "preview") loadFrame();
      }),
  );
  $("properties-jump").onclick = () => {
    $("element-form").closest("details").open = true;
    $("element-form").scrollIntoView({ behavior: "smooth", block: "start" });
  };
  $("zoom").onchange = draw;
  new ResizeObserver(() => {
    if (tab === "design") draw();
  }).observe($("stage-scroll"));
  $("window-form").onsubmit = $("element-form").onsubmit = (e) =>
    e.preventDefault();
  for (const id of [
    "title",
    "width",
    "height",
    "breakpoint",
    "background",
    "accent",
    "application-theme",
    "closeAction",
  ])
    $(id).onchange = () =>
      change(() => {
        if (["width", "height"].includes(id))
          project.sizes[mode][id] = Number($(id).value);
        else
          project[id === "application-theme" ? "theme" : id] = id === "breakpoint" ? Number($(id).value) : $(id).value;
      });
  for (const id of [
    "text",
    "value",
    "message",
    "checked",
    "kind",
    "minimum", "maximum", "progress", "percent",
    "color",
    "scale",
    "x",
    "y",
    "w",
    "h",
    "horizontal",
    "vertical",
  ])
    $(id).onchange = () =>
      change(() => {
        const e = element();
        if (!e) return;
        if (id === "h" && ["field", "checkbox"].includes(e.type)) e.customSize = true;
        if (["x", "y", "w", "h", "horizontal", "vertical"].includes(id))
          e.layouts[mode][id] = ["horizontal", "vertical"].includes(id)
            ? $(id).value
            : Number($(id).value);
        else
          e[id] =
            ["checked", "percent"].includes(id)
              ? $(id).checked
              : ["scale", "minimum", "maximum", "progress"].includes(id)
                ? Number($(id).value)
                : $(id).value;
      });
  function remove() {
    if (element())
      change(() => {
        project.elements = project.elements.filter((e) => e.id !== selected);
        selected = null;
      });
  }
  $("delete").onclick = remove;
  $("duplicate").onclick = () =>
    change(() => {
      const original = M.clone(element());
      const e = M.add(project, original.type);
      Object.assign(e, original, { id: e.id });
      for (const r of Object.values(e.layouts)) {
        r.x = Math.min(4096, r.x + 12);
        r.y = Math.min(4096, r.y + 12);
      }
      selected = e.id;
    });
  for (const [id, direction] of [
    ["backward", -1],
    ["forward", 1],
  ])
    $(id).onclick = () =>
      change(() => {
        const i = project.elements.findIndex((e) => e.id === selected),
          j = i + direction;
        if (i >= 0 && j >= 0 && j < project.elements.length)
          [project.elements[i], project.elements[j]] = [
            project.elements[j],
            project.elements[i],
          ];
      });
  function history(back) {
    const from = back ? undo : redo,
      to = back ? redo : undo;
    if (!from.length) return;
    to.push(M.clone(project));
    const nextId = project.nextId;
    project = from.pop();
    project.nextId = Math.max(nextId, project.nextId);
    persist();
    render();
  }
  $("undo").onclick = () => history(true);
  $("redo").onclick = () => history(false);
  document.addEventListener("keydown", (event) => {
    if (/INPUT|TEXTAREA|SELECT/.test(event.target.tagName)) return;
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "z") {
      event.preventDefault();
      history(!event.shiftKey);
      return;
    }
    if (tab !== "design" || !element()) return;
    if (event.key === "Delete") {
      event.preventDefault();
      remove();
      return;
    }
    if (!event.target.closest("#stage")) return;
    const deltas = {
      ArrowLeft: [-1, 0],
      ArrowRight: [1, 0],
      ArrowUp: [0, -1],
      ArrowDown: [0, 1],
    };
    if (deltas[event.key]) {
      event.preventDefault();
      change(() => {
        const r = element().layouts[mode],
          current = M.rect(project, element(), mode),
          d = deltas[event.key],
          n = event.shiftKey ? 10 : 1;
        r.x = Math.max(0, Math.min(4096, current.x + d[0] * n));
        r.y = Math.max(0, Math.min(4096, current.y + d[1] * n));
        r.horizontal = "left";
        r.vertical = "top";
      });
      $("stage").focus();
    }
  });
  $("stage").addEventListener("pointerdown", (event) => {
    const target = event.target.closest(".widget");
    if (drag || !target || event.button !== 0) return;
    event.preventDefault();
    const resize = event.target.classList.contains("handle");
    selected = target.dataset.id;
    render();
    const e = element(),
      r = M.rect(project, e, mode);
    drag = {
      id: event.pointerId,
      x: event.clientX,
      y: event.clientY,
      before: M.clone(project),
      rect: r,
      resize,
    };
    $("stage").setPointerCapture(event.pointerId);
    $("stage").focus({ preventScroll: true });
  });
  $("stage").addEventListener("pointermove", (event) => {
    if (!drag || event.pointerId !== drag.id) return;
    const dx = Math.round((event.clientX - drag.x) / scale),
      dy = Math.round((event.clientY - drag.y) / scale),
      r = element().layouts[mode],
      s = project.sizes[mode];
    if (drag.resize) {
      r.w = Math.max(8, Math.min(s.width - drag.rect.x, drag.rect.w + dx));
      if (element().type === "field") element().customSize = true;
      r.h = Math.max(8, Math.min(s.height - drag.rect.y, drag.rect.h + dy));
    } else {
      r.x = Math.max(0, Math.min(Math.max(0, s.width - r.w), drag.rect.x + dx));
      r.y = Math.max(
        0,
        Math.min(Math.max(0, s.height - r.h), drag.rect.y + dy),
      );
      r.horizontal = "left";
      r.vertical = "top";
    }
    draw();
  });
  function endDrag(event) {
    if (!drag || event.pointerId !== drag.id) return;
    const before = drag.before;
    if (event.type === "pointercancel") project = before;
    else if (JSON.stringify(project) !== JSON.stringify(before)) {
      undo.push(before);
      if (undo.length > 50) undo.shift();
      redo = [];
      project = M.validate(project);
      persist();
    }
    drag = null;
    render();
  }
  $("stage").addEventListener("pointerup", endDrag);
  $("stage").addEventListener("pointercancel", endDrag);
  function download(name, text, type) {
    const url = URL.createObjectURL(new Blob([text], { type })),
      a = document.createElement("a");
    a.href = url;
    a.download = name;
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  $("save").onclick = () =>
    download(
      "interface.skmui.json",
      JSON.stringify(project, null, 2),
      "application/json",
    );
  $("download").onclick = () =>
    download(Object.keys(M.files(project, mode))[0], project.main, "text/plain;charset=utf-8");
  $("download-layout").onclick = () =>
    download(
      Object.keys(M.files(project, mode))[1],
      M.generate(project, mode),
      "text/plain;charset=utf-8",
    );
  $("code-file").onchange = render;
  $("code").oninput = () => {
    if ($("code-file").value === "main.skm")
      change(() => {
        project.main = $("code").value;
      });
  };
  $("copy").onclick = async () => {
    try {
      await navigator.clipboard.writeText(
        ($("code-file").value === "main.skm" ? project.main : M.generate(project, mode)),
      );
      notice("Код скопирован");
    } catch {
      $("code").focus();
      $("code").select();
      notice("Выделен весь код — скопируйте его вручную");
    }
  };
  $("new").onclick = () => {
    if (
      !confirm(
        "Заменить весь проект, включая main.skm, шаблоном? Можно отменить через ↶.",
      )
    )
      return;
    change(() => {
      project = M.template($("template").value);
      selected = null;
    });
  };
  $("import").onclick = () => $("file").click();
  $("file").onchange = async () => {
    const file = $("file").files[0];
    if (!file) return;
    try {
      if (file.size > 250000) throw Error("Файл больше 250 КБ");
      const next = M.validate(JSON.parse(await file.text()));
      change(() => {
        project = next;
        selected = null;
      });
      notice("Проект открыт");
    } catch (error) {
      notice("Проект не изменён: " + error.message);
    }
    $("file").value = "";
  };
  function loadFrame() {
    if (!$("preview").getAttribute("src")) {
      $("preview").src = $("preview").dataset.src;
      $("run").disabled = true;
      $("preview-note").textContent = "Загрузка Skrumi…";
    }
  }
  window.addEventListener("message", (event) => {
    if (
      event.origin !== location.origin ||
      event.source !== $("preview").contentWindow
    )
      return;
    if (event.data?.type === "skrumi-builder-ready") {
      frameReady = true;
      $("run").disabled = false;
      $("preview-note").textContent = "Skrumi готов к запуску.";
    }
    if (
      event.data?.type === "skrumi-builder-height" &&
      Number.isFinite(event.data.height)
    )
      $("preview").style.height =
        Math.max(300, Math.min(2600, event.data.height)) + "px";
  });
  $("run").onclick = () => {
    if (!frameReady) {
      notice("Дождитесь загрузки Skrumi");
      return;
    }
    $("preview").contentWindow.postMessage(
      { type: "skrumi-builder-run", files: M.files(project, mode) },
      location.origin,
    );
    frameStarted = true;
    $("preview-note").textContent = "Выполняется сгенерированная программа";
  };
  $("stop").onclick = () => {
    if (frameReady)
      $("preview").contentWindow.postMessage(
        { type: "skrumi-builder-stop" },
        location.origin,
      );
    $("preview-note").textContent = "Остановлено";
  };
  project = M.validate(project);
  render();
})();
