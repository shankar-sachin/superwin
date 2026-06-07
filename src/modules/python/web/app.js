// SuperWin Python IDE front-end. A lightweight, dependency-free editor: a
// transparent <textarea> for input layered over a syntax-highlighted <pre>
// overlay (regex tokenizer). "Run" posts the source to the native host, which
// spawns the system Python and streams stdout/stderr back via swAppendOutput.
(function () {
  "use strict";

  var KEYWORDS = ("False None True and as assert async await break class continue def del " +
    "elif else except finally for from global if import in is lambda nonlocal not or pass " +
    "raise return try while with yield match case").split(" ");
  var BUILTINS = ("print len range int float str list dict set tuple bool abs min max sum " +
    "sorted enumerate zip map filter open input type isinstance super object Exception " +
    "round pow divmod repr format reversed any all bytes bytearray").split(" ");
  var KW = {}, BI = {};
  KEYWORDS.forEach(function (k) { KW[k] = 1; });
  BUILTINS.forEach(function (k) { BI[k] = 1; });

  function esc(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  // Tokenize Python into highlighted HTML. Order matters: comments and strings
  // (including triple-quoted, which may span lines) are matched before words.
  var TOKEN = /(#[^\n]*)|("""[\s\S]*?"""|'''[\s\S]*?'''|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')|(\b\d+\.?\d*(?:[eE][+-]?\d+)?\b)|(@[A-Za-z_]\w*)|(\b[A-Za-z_]\w*\b)/g;

  function highlight(src) {
    var out = "", last = 0, m;
    TOKEN.lastIndex = 0;
    while ((m = TOKEN.exec(src))) {
      out += esc(src.slice(last, m.index));
      if (m[1]) out += '<span class="c">' + esc(m[1]) + "</span>";
      else if (m[2]) out += '<span class="s">' + esc(m[2]) + "</span>";
      else if (m[3]) out += '<span class="n">' + esc(m[3]) + "</span>";
      else if (m[4]) out += '<span class="d">' + esc(m[4]) + "</span>";
      else if (m[5]) {
        var w = m[5];
        if (KW[w]) out += '<span class="k">' + w + "</span>";
        else if (BI[w]) out += '<span class="b">' + w + "</span>";
        else out += esc(w);
      }
      last = TOKEN.lastIndex;
    }
    out += esc(src.slice(last));
    return out + "\n";  // trailing newline keeps the last line visible
  }

  var code, hl, out, statusEl, running = false;

  function sync() {
    hl.firstChild.innerHTML = highlight(code.value);
    hl.scrollTop = code.scrollTop;
    hl.scrollLeft = code.scrollLeft;
  }

  function post(msg) {
    if (window.chrome && window.chrome.webview) window.chrome.webview.postMessage(JSON.stringify(msg));
  }

  // ---- host -> page API ----
  window.swSetTheme = function (t) { document.body.className = (t === "light" ? "light" : "dark"); };
  window.swAppendOutput = function (text) {
    out.appendChild(document.createTextNode(text));
    out.scrollTop = out.scrollHeight;
  };
  window.swClearOutput = function () { out.textContent = ""; };
  window.swRunState = function (isRunning) {
    running = isRunning;
    document.getElementById("run").textContent = isRunning ? "■ Running…" : "▶ Run";
    document.getElementById("run").disabled = isRunning;
    statusEl.textContent = isRunning ? "" : statusEl.textContent;
  };
  window.swSetStatus = function (s) { statusEl.textContent = s || ""; };
  window.swSetCode = function (s) { code.value = s || ""; sync(); };

  document.addEventListener("DOMContentLoaded", function () {
    code = document.getElementById("code");
    hl = document.getElementById("hl");
    out = document.getElementById("out");
    statusEl = document.getElementById("status");

    code.value = "# Write Python here, then press Run.\nfor i in range(5):\n    print('hello', i)\n";
    sync();

    code.addEventListener("input", sync);
    code.addEventListener("scroll", function () { hl.scrollTop = code.scrollTop; hl.scrollLeft = code.scrollLeft; });
    code.addEventListener("keydown", function (e) {
      if (e.key === "Tab") {
        e.preventDefault();
        var s = code.selectionStart, en = code.selectionEnd;
        code.value = code.value.slice(0, s) + "    " + code.value.slice(en);
        code.selectionStart = code.selectionEnd = s + 4;
        sync();
      }
    });

    document.getElementById("run").addEventListener("click", function () {
      if (running) return;
      out.textContent = "";
      post({ type: "run", code: code.value });
    });
    document.getElementById("clear").addEventListener("click", function () { out.textContent = ""; });
  });
})();
