/* Project format and Skrumi generator, shared by the browser and runtime tests. */
(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.FoxBuilder = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";
  const types = {
    label: "Подпись",
    button: "Кнопка",
    primary: "Основная кнопка",
    field: "Поле ввода",
    checkbox: "Флажок",
    panel: "Панель",
    progress: "Прогресс",
    dialog: "Диалог",
  };
  const clone = (value) => JSON.parse(JSON.stringify(value));
  function fail(message) {
    throw new Error(message);
  }
  function integer(value, min, max, name) {
    if (!Number.isInteger(value) || value < min || value > max)
      fail(`${name}: целое число от ${min} до ${max}`);
    return value;
  }
  function text(value, limit, name) {
    if (
      typeof value !== "string" ||
      value.length > limit ||
      /[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]/.test(value)
    )
      fail(`${name}: недопустимый текст (максимум ${limit} символов)`);
    if (
      [...value].some(
        (ch) =>
          ch.length === 1 &&
          ch.charCodeAt(0) >= 0xd800 &&
          ch.charCodeAt(0) <= 0xdfff,
      )
    )
      fail("Некорректный Unicode");
    return value;
  }
  function color(value) {
    if (typeof value !== "string" || !/^#[0-9a-f]{6}$/i.test(value))
      fail("Цвет должен иметь вид #rrggbb");
    return value.toLowerCase();
  }
  function defaultMain() {
    return [
      "// Пользовательская логика: конструктор не перезаписывает этот файл.",
      'include("layout.skm");',
      "",
      "void on_click(string id) {",
      '    // Пример: if (id == "e5") { print(value_e3); return; }',
      "    // ID выбранного элемента указан в его свойствах.",
      "    print(layout_message(id));",
      "}",
      "",
      "layout_open();",
      "while (window_poll()) {",
      "    string clicked = layout_draw();",
      '    if (clicked != "") { on_click(clicked); }',
      "    present_window();",
      "}",
      "close_window();",
      "",
    ].join("\n");
  }
  function validate(input) {
    if (!input || input.format !== "foxlang-ui" || input.version !== 1)
      fail("Нужен проект Skrumi UI формата 1");
    if (
      !input.sizes ||
      !Array.isArray(input.elements) ||
      input.elements.length > 100
    )
      fail("В проекте может быть до 100 элементов");
    const sizes = {};
    for (const mode of ["desktop", "mobile"]) {
      const size = input.sizes[mode] || {};
      sizes[mode] = {
        width: integer(size.width, 160, 1920, "Ширина окна"),
        height: integer(size.height, 160, 1080, "Высота окна"),
      };
    }
    const breakpoint = integer(
      input.breakpoint,
      200,
      1200,
      "Порог мобильного макета",
    );
    if (sizes.mobile.width >= breakpoint || sizes.desktop.width < breakpoint)
      fail(
        "Ширина мобильного окна должна быть меньше порога, а ПК — не меньше",
      );
    const ids = new Set();
    const elements = input.elements.map((e) => {
      if (
        !e ||
        typeof e.type !== "string" ||
        !Object.hasOwn(types, e.type) ||
        typeof e.id !== "string" ||
        !/^e[1-9][0-9]{0,5}$/.test(e.id) ||
        ids.has(e.id)
      )
        fail("Неизвестный элемент или повторяющийся ID");
      ids.add(e.id);
      if (typeof e.checked !== "boolean")
        fail("Состояние флажка должно быть true или false");
      const layouts = {};
      for (const mode of ["desktop", "mobile"]) {
        const r = e.layouts?.[mode] || {};
        if (
          !["left", "center", "right", "stretch"].includes(r.horizontal) ||
          !["top", "bottom"].includes(r.vertical)
        )
          fail("Неизвестная привязка");
        layouts[mode] = {
          x: integer(r.x, 0, 4096, "X"),
          y: integer(r.y, 0, 4096, "Y"),
          w: integer(r.w, 8, 4096, "Ширина"),
          h: integer(r.h, 8, 4096, "Высота"),
          horizontal: r.horizontal,
          vertical: r.vertical,
        };
        if (e.type === "field" && !e.customSize) layouts[mode].h = 28;
        if (e.type === "checkbox" && !e.customSize) layouts[mode].h = 14;
      }
      if (e.kind !== undefined && !["info", "error", "confirm"].includes(e.kind)) fail("Неизвестный вид диалога");
      if (e.type === "dialog" && Object.values(layouts).some(r => r.w < 144 || r.h < 100)) fail("Диалог: минимум 144×100");
      if (e.customSize !== undefined && typeof e.customSize !== "boolean") fail("customSize: true или false");
      const label = text(e.text, 200, "Подпись");
      const minimum = integer(e.minimum ?? 0, -1000000, 1000000, "Минимум");
      const maximum = integer(e.maximum ?? 100, -1000000, 1000000, "Максимум");
      if (maximum <= minimum) fail("Максимум должен быть больше минимума");
      if (e.percent !== undefined && typeof e.percent !== "boolean") fail("Проценты: true или false");
      const labelScale = integer(e.scale, 1, 4, "Масштаб текста");
      if (["label", "checkbox"].includes(e.type)) {
        const factor = e.type === "label" ? labelScale : 1;
        const width = Math.max(
          0,
          ...label
            .split("\n")
            .map((line) => [...line].length * 6 * factor - factor),
        );
        for (const r of Object.values(layouts)) {
          r.w = Math.min(
            4096,
            Math.max(8, width + (e.type === "checkbox" ? r.h + Math.max(3, Math.floor(r.h * 6 / 14)) : 0)),
          );
          r.h =
            e.type === "checkbox"
              ? r.h
              : Math.min(
                  4096,
                  Math.max(8, label.split("\n").length * 8 * factor),
                );
          if (r.horizontal === "stretch") r.horizontal = "left";
        }
      }
      return {
        id: e.id,
        type: e.type,
        text: label,
        value: text(e.value, 500, "Текст поля"),
        message: text(e.message, 500, "Сообщение"),
        checked: e.checked,
        customSize: e.customSize ?? false,
        kind: e.kind ?? "info",
        minimum, maximum,
        progress: integer(e.progress ?? 0, -1000000, 1000000, "Прогресс"),
        percent: e.percent ?? true,
        color: color(e.color),
        scale: integer(e.scale, 1, 4, "Масштаб текста"),
        layouts,
      };
    });
    const minimumId =
      Math.max(0, ...elements.map((e) => Number(e.id.slice(1)))) + 1;
    const nextId =
      input.nextId === undefined
        ? minimumId
        : integer(input.nextId, minimumId, 1000000, "Следующий ID");
    if (input.theme !== undefined && !["legacy", "light", "dark"].includes(input.theme)) fail("Неизвестная тема");
    if (input.closeAction !== undefined && !["default", "confirm", "event"].includes(input.closeAction)) fail("Неизвестное событие закрытия");
    const styles = {};
    const styleNames = ["text", "muted", "border", "field", "button", "buttonHover", "panel", "track"];
    for (const [key, value] of Object.entries(input.styles ?? {})) {
      if (!styleNames.includes(key)) fail("Неизвестное свойство стиля");
      styles[key] = color(value);
    }
    return {
      main:
        input.main === undefined
          ? defaultMain()
          : text(input.main, 100000, "main.skm"),
      nextId,
      format: "foxlang-ui",
      version: 1,
      title: text(input.title, 120, "Название окна") || "Мой интерфейс",
      sizes,
      breakpoint,
      background: color(input.background),
      accent: color(input.accent),
      theme: input.theme ?? "legacy",
      closeAction: input.closeAction ?? "default",
      styles,
      elements,
    };
  }
  function blank() {
    return {
      main: defaultMain(),
      nextId: 1,
      format: "foxlang-ui",
      version: 1,
      title: "Мой интерфейс",
      sizes: {
        desktop: { width: 800, height: 560 },
        mobile: { width: 360, height: 640 },
      },
      breakpoint: 600,
      background: "#f3f5f8",
      accent: "#2d69af",
      theme: "legacy",
      closeAction: "default",
      styles: {},
      elements: [],
    };
  }
  function add(project, type) {
    if (!Object.hasOwn(types, type) || project.elements.length >= 100)
      fail("Нельзя добавить элемент");
    const next = Math.max(
      project.nextId || 1,
      ...project.elements.map((e) => Number(e.id.slice(1)) + 1),
    );
    if (next > 999999) fail("Исчерпаны ID элементов: создайте новый проект");
    project.nextId = next + 1;
    const id = "e" + next;
    const height =
      { label: 24, field: 28, checkbox: 14, panel: 160, dialog: 220 }[type] || 36;
    const width =
      { label: 240, field: 280, checkbox: 200, panel: 320, dialog: 520 }[type] || 160;
    const e = {
      id,
      type,
      text:
        {
          label: "Новая подпись",
          field: "Введите текст",
          checkbox: "Включить опцию",
          panel: "Панель",
        }[type] || "Продолжить",
      value: "",
      message: "Кнопка нажата",
      checked: false,
      kind: "info",
      minimum: 0, maximum: 100, progress: 0, percent: true,
      color: type === "panel" ? "#ffffff" : "#1e232a",
      scale: type === "label" ? 2 : 1,
      layouts: {},
    };
    for (const mode of ["desktop", "mobile"])
      e.layouts[mode] = {
        x: 24,
        y: Math.max(
          0,
          Math.min(
            project.sizes[mode].height - height - 24,
            24 + (project.elements.length % 7) * 52,
          ),
        ),
        w: Math.min(width, project.sizes[mode].width - 48),
        h: height,
        horizontal: ["field", "panel"].includes(type) ? "stretch" : "left",
        vertical: "top",
      };
    project.elements.push(e);
    return e;
  }
  function template(name = "welcome") {
    const p = blank();
    if (name === "blank") return p;
    p.title = name === "login" ? "Вход в приложение" : "Добро пожаловать";
    const title = add(p, "label");
    title.text = p.title;
    const subtitle = add(p, "label");
    subtitle.scale = 1;
    subtitle.text = "Создано в Skrumi";
    subtitle.color = "#78808a";
    const field = add(p, "field");
    field.text = name === "login" ? "Ваш логин" : "Как вас зовут?";
    const check = add(p, "checkbox");
    check.text = "Запомнить меня";
    const button = add(p, "primary");
    button.text = name === "login" ? "Войти" : "Продолжить";
    return p;
  }
  function rect(
    project,
    element,
    mode,
    width = project.sizes[mode].width,
    height = project.sizes[mode].height,
  ) {
    const r = element.layouts[mode],
      base = project.sizes[mode];
    const right = Math.max(0, base.width - r.x - r.w),
      bottom = Math.max(0, base.height - r.y - r.h);
    const w =
      r.horizontal === "stretch" ? Math.max(8, width - r.x - right) : r.w;
    const x =
      r.horizontal === "right"
        ? Math.max(0, width - right - w)
        : r.horizontal === "center"
          ? Math.max(0, Math.floor((width - w) / 2))
          : r.x;
    const y =
      r.vertical === "bottom" ? Math.max(0, height - bottom - r.h) : r.y;
    return { x, y, w, h: r.h };
  }
  const quote = (value) => JSON.stringify(value);
  const rgb = (value) =>
    `rgb(${[1, 3, 5].map((i) => parseInt(value.slice(i, i + 2), 16)).join(", ")})`;
  function generate(input, initialMode = "desktop") {
    const p = validate(input),
      initial = p.sizes[initialMode];
    if (!initial) fail("Неизвестный размер запуска");
    const lines = [
      "// Создано в Skrumi UI — https://skrinvex.github.io/Skrumi/builder/",
      "// Генерируемый layout.skm. Изменяйте обработчики в main.skm.",
      "using graphics;",
      "using ui;",
      "",
      "bool layout_close_pending = false;",
      "bool layout_close_notified = false;",
      "void layout_accept_close() { intercept_window_close(false); }",
      "void layout_reject_close() { cancel_window_close(); layout_close_notified = false; }",
      "void layout_open() {",
      `    open_window(${initial.width}, ${initial.height}, ${quote(p.title)});`,
      "    set_window_resizable(true);",
      ...(p.theme === "legacy" ? [] : [`    ui_theme(${p.theme === "dark"});`]),
      `    UI_ACCENT = ${rgb(p.accent)};`,
      ...(p.closeAction === "default" ? [] : ["    intercept_window_close(true);"]),
      ...Object.entries(p.styles).map(([key, value]) => `    UI_${key === "buttonHover" ? "BUTTON_HOVER" : key.toUpperCase()} = ${rgb(value)};`),
      "}",
      "",
    ];
    for (const e of p.elements) {
      if (e.type === "dialog") lines.push(`bool open_${e.id} = ${e.checked};`, `string result_${e.id} = "pending";`);
      if (e.type === "field")
        lines.push(`string value_${e.id} = ${quote(e.value)};`);
      if (e.type === "checkbox")
        lines.push(`bool checked_${e.id} = ${e.checked};`);
      if (e.type === "progress")
        lines.push(`float progress_${e.id} = ${e.progress};`);
    }
    lines.push(
      "",
      "string layout_draw() {",
      '    string clicked = "";',
      `    clear_window(${rgb(p.background)});`,
      `    if (window_width() < ${p.breakpoint}) {`,
    );
    for (const mode of ["mobile", "desktop"]) {
      if (mode === "desktop") lines.push("    } else {");
      for (const e of [...p.elements.filter(e => e.type !== "dialog"), ...p.elements.filter(e => e.type === "dialog")]) {
        const r = e.layouts[mode],
          base = p.sizes[mode],
          right = Math.max(0, base.width - r.x - r.w),
          bottom = Math.max(0, base.height - r.y - r.h);
        const w =
          r.horizontal === "stretch"
            ? `max(8, window_width() - ${r.x + right})`
            : String(r.w);
        const x =
          r.horizontal === "right"
            ? `max(0, window_width() - ${right + r.w})`
            : r.horizontal === "center"
              ? `max(0, (window_width() - ${r.w}) / 2)`
              : String(r.x);
        const y =
          r.vertical === "bottom"
            ? `max(0, window_height() - ${bottom + r.h})`
            : String(r.y);
        const args = `${x}, ${y}, ${w}`,
          id = quote(e.id);
        lines.push(`        // ${e.id}: ${types[e.type]}`);
        if (e.type === "dialog") lines.push(
          `        if (open_${e.id}) {`,
          `            result_${e.id} = ui_dialog_box(${id}, ${args}, ${r.h}, ${quote(e.text)}, ${quote(e.message)}, ${quote(e.kind)});`,
          `            if (result_${e.id} != "pending") { open_${e.id} = false; clicked = ${quote(e.id + "#")} + result_${e.id}; }`,
          "        }",
        );
        if (e.type === "label")
          lines.push(
            `        draw_text(${x}, ${y}, ${quote(e.text)}, ${e.scale}, ${rgb(e.color)});`,
          );
        if (e.type === "panel")
          lines.push(`        draw_rect(${args}, ${r.h}, ${rgb(e.color)});`);
        if (e.type === "progress")
          lines.push(`        ui_progress_bar(${args}, ${r.h}, progress_${e.id}, ${e.minimum}, ${e.maximum}, ${e.percent});`);
        if (e.type === "field")
          lines.push(
            `        value_${e.id} = ui_text_field_box(${id}, ${args}, ${r.h}, value_${e.id}, ${quote(e.text)});`,
          );
        if (e.type === "checkbox")
          lines.push(
            `        checked_${e.id} = ui_checkbox_box(${id}, ${x}, ${y}, ${r.h}, ${quote(e.text)}, checked_${e.id});`,
          );
        if (["button", "primary"].includes(e.type))
          lines.push(
            `        if (${e.type === "primary" ? "ui_primary_button" : "ui_button"}(${id}, ${args}, ${r.h}, ${quote(e.text)})) {`,
            `            clicked = ${id};`,
            "        }",
          );
      }
    }
    lines.push(
      "    }",
      "    layout_close_pending = window_close_requested();",
      "    if (!layout_close_pending) { layout_close_notified = false; }",
      ...(p.closeAction === "confirm" ? [
        '    if (layout_close_pending) {',
        `        string answer = ui_dialog("#layout-close", ${quote(p.title)}, "Закрыть приложение?", "confirm");`,
        '        if (answer == "yes") { layout_accept_close(); }',
        '        else if (answer == "no" || answer == "cancel") { layout_reject_close(); }',
        '    }',
      ] : p.closeAction === "event" ? [
        '    if (layout_close_pending && !layout_close_notified) { clicked = "#close"; layout_close_notified = true; }',
      ] : []),
      "    return clicked;",
      "}",
      "",
      "string layout_message(string id) {",
    );
    for (const e of p.elements)
      if (["button", "primary"].includes(e.type))
        lines.push(
          `    if (id == ${quote(e.id)}) { return ${quote(e.message)}; }`,
        );
    lines.push('    return "";', "}", "");
    return lines.join("\n");
  }
  function files(input, initialMode = "desktop") {
    const p = validate(input);
    const legacy = /include\s*\(\s*["']layout\.fox["']/.test(p.main);
    return legacy ? { "main.fox": p.main, "layout.fox": generate(p, initialMode) } :
      { "main.skm": p.main, "layout.skm": generate(p, initialMode) };
  }
  return {
    types,
    clone,
    validate,
    blank,
    add,
    template,
    rect,
    generate,
    files,
    defaultMain,
  };
});
