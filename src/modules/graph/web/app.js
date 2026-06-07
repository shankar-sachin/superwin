// SuperWin Graphing Calculator — MathLive editor front-end.
//
// Hosts a list of editable LaTeX math-fields. On every change it posts the full
// state (latex + colour + visibility per row) to the native host, which converts
// the LaTeX to its CAS grammar and plots on a native canvas. The host can drive
// us back via swInit / swAddRow / swSetTheme (called through ExecuteScriptAsync).
(function () {
  "use strict";

  var PALETTE = ["#4f8ef7", "#ff454a", "#34c759", "#ff9f0a", "#af52de", "#5ac8fa"];
  var rows = [];
  var nextId = 1;
  var activeMf = null;  // the math-field that last had focus (insert target)

  function rowsEl() { return document.getElementById("rows"); }

  // Insert a LaTeX snippet into the focused (or last) equation field. Used by the
  // toolbar buttons for √, fractions, Σ, Π, ∫ and d/dx so these are reachable
  // without memorising LaTeX. Jumps the cursor to the first \placeholder{}.
  function insertLatex(latex) {
    var mf = activeMf;
    if (!mf) {
      var all = document.querySelectorAll("math-field.mf");
      mf = all[all.length - 1];
    }
    if (!mf) return;
    mf.focus();
    mf.insert(latex, { focus: true, selectionMode: "placeholder" });
  }

  function post() {
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(JSON.stringify({
        type: "state",
        rows: rows.map(function (r) {
          return { id: r.id, latex: r.latex, color: r.color, visible: r.visible };
        })
      }));
    }
  }

  function makeRow(data) {
    data = data || {};
    var r = {
      id: nextId++,
      latex: data.latex || "",
      color: data.color || PALETTE[rows.length % PALETTE.length],
      visible: data.visible !== false
    };
    rows.push(r);
    renderRow(r);
    post();
  }

  function renderRow(r) {
    var row = document.createElement("div");
    row.className = "row" + (r.visible ? "" : " hidden");
    row.dataset.id = r.id;
    row.style.setProperty("--c", r.color);

    var color = document.createElement("input");
    color.type = "color";
    color.value = r.color;
    color.className = "swatch";
    color.title = "Pick this curve's colour";
    color.addEventListener("input", function () {
      r.color = color.value;
      row.style.setProperty("--c", r.color);
      post();
    });

    var mf = document.createElement("math-field");
    mf.className = "mf";
    mf.setAttribute("math-virtual-keyboard-policy", "manual");
    // smartMode auto-commits a single-character superscript, which kicks the
    // caret back down to the baseline after one digit -- so x^2 then 3 became
    // x^2·3 instead of x^23. Leaving it off keeps the caret in the exponent so
    // multi-digit powers like x^234 work; inline shortcuts (sin, cos, ...) still
    // expand. Press the Right arrow to leave the exponent, as on a TI-30XIIS.
    if ("smartMode" in mf) mf.smartMode = false;
    mf.value = r.latex;
    mf.addEventListener("input", function () {
      r.latex = mf.value;
      post();
    });
    mf.addEventListener("focusin", function () { activeMf = mf; });

    var eye = document.createElement("button");
    eye.type = "button";
    eye.className = "icon";
    eye.textContent = r.visible ? "◉" : "○"; // ◉ / ○
    eye.title = "Show / hide this curve";
    eye.addEventListener("click", function () {
      r.visible = !r.visible;
      eye.textContent = r.visible ? "◉" : "○";
      row.classList.toggle("hidden", !r.visible);
      post();
    });

    var del = document.createElement("button");
    del.type = "button";
    del.className = "icon";
    del.textContent = "✕"; // ✕
    del.title = "Delete this expression";
    del.addEventListener("click", function () {
      rows = rows.filter(function (x) { return x !== r; });
      row.remove();
      post();
    });

    row.appendChild(color);
    row.appendChild(mf);
    row.appendChild(eye);
    row.appendChild(del);
    rowsEl().appendChild(row);
  }

  // ---- host -> page API (invoked via ExecuteScriptAsync) ----
  window.swAddRow = function () { makeRow(); };
  window.swSetTheme = function (t) { document.body.className = (t === "light" ? "light" : "dark"); };
  window.swInit = function (json) {
    try {
      var arr = JSON.parse(json);
      rowsEl().innerHTML = "";
      rows = [];
      arr.forEach(function (d) { makeRow(d); });
    } catch (e) { /* ignore */ }
  };

  document.addEventListener("DOMContentLoaded", function () {
    if (window.MathfieldElement) {
      MathfieldElement.fontsDirectory = "./fonts";
      MathfieldElement.soundsDirectory = null;
    }
    document.getElementById("add").addEventListener("click", function () { makeRow(); });

    // Insert toolbar: each button drops a LaTeX template into the focused field.
    var TOOLS = [
      { id: "ins-frac",  latex: "\\frac{\\placeholder{}}{\\placeholder{}}" },
      { id: "ins-sqrt",  latex: "\\sqrt{\\placeholder{}}" },
      { id: "ins-pow",   latex: "^{\\placeholder{}}" },
      { id: "ins-sum",   latex: "\\sum_{n=1}^{10}\\placeholder{}" },
      { id: "ins-prod",  latex: "\\prod_{n=1}^{5}\\placeholder{}" },
      { id: "ins-int",   latex: "\\int_{0}^{x}\\placeholder{}\\,dx" },
      { id: "ins-deriv", latex: "\\frac{\\mathrm{d}}{\\mathrm{d}x}\\left(\\placeholder{}\\right)" },
      { id: "ins-pi",    latex: "\\pi" }
    ];
    TOOLS.forEach(function (t) {
      var el = document.getElementById(t.id);
      if (el) el.addEventListener("click", function () { insertLatex(t.latex); });
    });

    makeRow({ latex: "\\sin(x)" });  // a friendly starting curve
  });
})();
