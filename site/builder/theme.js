/* Apply the saved editor theme before styles load, without changing project colors. */
(() => {
  "use strict";
  const key = "skrumi-builder-theme";
  const system = matchMedia("(prefers-color-scheme: dark)");
  let preference = "auto";
  try {
    const saved = localStorage.getItem(key);
    if (["auto", "light", "dark"].includes(saved)) preference = saved;
  } catch (_) { /* Storage may be disabled. */ }
  function apply() {
    document.documentElement.dataset.theme = preference === "auto"
      ? (system.matches ? "dark" : "light") : preference;
  }
  apply();
  system.addEventListener("change", apply);
  document.addEventListener("DOMContentLoaded", () => {
    const control = document.getElementById("editor-theme");
    control.value = preference;
    control.addEventListener("change", () => {
      preference = control.value;
      try { localStorage.setItem(key, preference); } catch (_) { /* Optional. */ }
      apply();
    });
  });
})();
