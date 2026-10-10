/* Project format and FoxLang generator, shared by the browser and runtime tests. */
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
      /[\x00-\x08\x0b-\x1f\x7f]/.test(value)
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
  function validate(input) {
    if (!input || input.format !== "foxlang-ui" || input.version !== 1)
      fail("Нужен проект FoxLang UI формата 1");
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
        if (e.type === "field") layouts[mode].h = 28;
        if (e.type === "checkbox") layouts[mode].h = 14;
      }
      const label = text(e.text, 200, "Подпись");
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
            Math.max(8, width + (e.type === "checkbox" ? 20 : 0)),
          );
          r.h =
            e.type === "checkbox"
              ? 14
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
        color: color(e.color),
        scale: integer(e.scale, 1, 4, "Масштаб текста"),
        layouts,
      };
    });
    return {
      format: "foxlang-ui",
      version: 1,
      title: text(input.title, 120, "Название окна") || "Мой интерфейс",
      sizes,
      breakpoint,
      background: color(input.background),
      accent: color(input.accent),
      elements,
    };
  }
  function blank() {
    return {
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
      elements: [],
    };
  }
  function add(project, type) {
    if (!Object.hasOwn(types, type) || project.elements.length >= 100)
      fail("Нельзя добавить элемент");
    let next = 1;
    while (project.elements.some((e) => e.id === "e" + next)) next++;
    const id = "e" + next;
    const height =
      { label: 24, field: 28, checkbox: 14, panel: 160 }[type] || 36;
    const width =
      { label: 240, field: 280, checkbox: 200, panel: 320 }[type] || 160;
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
    subtitle.text = "Создано в FoxLang";
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
      "// Создано в FoxLang UI — https://skrinvex.github.io/FoxLang/builder/",
      "// Состояние хранится между кадрами. Логику кнопок добавьте в обработчики ниже.",
      "using graphics;",
      "using ui;",
      "",
      `open_window(${initial.width}, ${initial.height}, ${quote(p.title)});`,
      "set_window_resizable(true);",
      `UI_ACCENT = ${rgb(p.accent)};`,
      "",
    ];
    for (const e of p.elements) {
      if (e.type === "field")
        lines.push(`string value_${e.id} = ${quote(e.value)};`);
      if (e.type === "checkbox")
        lines.push(`bool checked_${e.id} = ${e.checked};`);
    }
    lines.push(
      "",
      "while (window_poll()) {",
      `    clear_window(${rgb(p.background)});`,
      `    if (window_width() < ${p.breakpoint}) {`,
    );
    for (const mode of ["mobile", "desktop"]) {
      if (mode === "desktop") lines.push("    } else {");
      for (const e of p.elements) {
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
        if (e.type === "label")
          lines.push(
            `        draw_text(${x}, ${y}, ${quote(e.text)}, ${e.scale}, ${rgb(e.color)});`,
          );
        if (e.type === "panel")
          lines.push(`        draw_rect(${args}, ${r.h}, ${rgb(e.color)});`);
        if (e.type === "field")
          lines.push(
            `        value_${e.id} = ui_text_field(${id}, ${args}, value_${e.id}, ${quote(e.text)});`,
          );
        if (e.type === "checkbox")
          lines.push(
            `        checked_${e.id} = ui_checkbox(${id}, ${x}, ${y}, ${quote(e.text)}, checked_${e.id});`,
          );
        if (["button", "primary"].includes(e.type))
          lines.push(
            `        if (${e.type === "primary" ? "ui_primary_button" : "ui_button"}(${id}, ${args}, ${r.h}, ${quote(e.text)})) {`,
            "            // Ваш обработчик нажатия.",
            `            print(${quote(e.message)});`,
            "        }",
          );
      }
    }
    lines.push("    }", "    present_window();", "}", "close_window();", "");
    return lines.join("\n");
  }
  return { types, clone, validate, blank, add, template, rect, generate };
});
