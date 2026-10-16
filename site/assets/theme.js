/* Shared theme for the guide, playground and builder; runs before first paint. */
(() => {
  "use strict";
  const key = "skrumi-theme", system = matchMedia("(prefers-color-scheme: dark)");
  const valid = value => ["auto", "light", "dark"].includes(value);
  let preference = "auto";
  try {
    const saved = localStorage.getItem(key);
    const legacy = localStorage.getItem(location.pathname.includes("/builder") ? "skrumi-builder-theme" : "foxlang-playground-theme");
    preference = valid(saved) ? saved : valid(legacy) ? legacy : "auto";
    if (!valid(saved)) localStorage.setItem(key, preference);
  } catch (_) { /* Themes also work without storage. */ }
  const icons = {
    light: "M12 7a5 5 0 1 0 0 10a5 5 0 0 0 0-10z M12 1v2 M12 21v2 M4.2 4.2l1.4 1.4 M18.4 18.4l1.4 1.4 M1 12h2 M21 12h2 M4.2 19.8l1.4-1.4 M18.4 5.6l1.4-1.4",
    dark: "M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"
  };
  function apply() {
    const theme = preference === "auto" ? (system.matches ? "dark" : "light") : preference;
    document.documentElement.dataset.theme = theme;
    document.documentElement.dataset.themePreference = preference;
    const button = document.getElementById("theme");
    if (!button) return;
    button.querySelector("path").setAttribute("d", icons[theme]);
    const label = theme === "dark" ? "Включить светлую тему" : "Включить тёмную тему";
    button.setAttribute("aria-label", label);
    button.title = label + ". Shift + нажатие — как в системе";
  }
  apply();
  system.addEventListener("change", apply);
  addEventListener("storage", event => {
    if (event.key === key || event.key === null) {
      preference = valid(event.newValue) ? event.newValue : "auto";
      apply();
    }
  });
  document.addEventListener("DOMContentLoaded", () => {
    apply();
    document.getElementById("theme").addEventListener("click", event => {
      preference = event.shiftKey ? "auto" : document.documentElement.dataset.theme === "dark" ? "light" : "dark";
      try { localStorage.setItem(key, preference); } catch (_) { /* Optional. */ }
      apply();
    });
  });
})();
