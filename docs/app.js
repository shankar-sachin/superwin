// SuperWin landing page — interactivity + live version from GitHub Releases.
(function () {
  "use strict";

  var REPO = "shankar-sachin/superwin";
  var FALLBACK_VERSION = "2.3.0"; // keep in sync with src/Version.h

  // ---- Footer year ----
  var yearEl = document.getElementById("year");
  if (yearEl) yearEl.textContent = String(new Date().getFullYear());

  // ---- Scroll reveal ----
  var revealEls = document.querySelectorAll(".reveal");
  if ("IntersectionObserver" in window) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) {
          e.target.classList.add("in");
          io.unobserve(e.target);
        }
      });
    }, { threshold: 0.12 });
    revealEls.forEach(function (el) { io.observe(el); });
  } else {
    revealEls.forEach(function (el) { el.classList.add("in"); });
  }

  // ---- Apply a version (string like "2.1.4") + optional direct asset URL ----
  function applyVersion(version, assetUrl) {
    var v = "v" + version;
    ["navVersion", "footVersion"].forEach(function (id) {
      var el = document.getElementById(id);
      if (el) el.textContent = v;
    });

    var subText = "SuperWin " + v + " · 64-bit · no admin";
    var sub = document.getElementById("downloadSub");
    if (sub) sub.textContent = subText;
    var sub2 = document.getElementById("downloadSub2");
    if (sub2) sub2.textContent = v + " for Windows 10/11";

    var name = document.getElementById("installName");
    if (name) name.textContent = "SuperWin_" + v + ".exe";

    // Point the download buttons at the actual asset when we know it; otherwise
    // the static href (releases/latest) is already a sensible default.
    if (assetUrl) {
      ["downloadBtn", "downloadBtn2"].forEach(function (id) {
        var el = document.getElementById(id);
        if (el) el.href = assetUrl;
      });
    }
  }

  // Seed with the known version immediately so the page is correct offline.
  applyVersion(FALLBACK_VERSION, null);

  // ---- Try to upgrade to whatever the latest GitHub release actually is ----
  fetch("https://api.github.com/repos/" + REPO + "/releases/latest", {
    headers: { Accept: "application/vnd.github+json" }
  })
    .then(function (r) { return r.ok ? r.json() : Promise.reject(r.status); })
    .then(function (data) {
      var tag = (data.tag_name || "").replace(/^v/, "");
      if (!tag) return;
      var exe = (data.assets || []).filter(function (a) {
        return /\.exe$/i.test(a.name);
      })[0];
      applyVersion(tag, exe ? exe.browser_download_url : null);
    })
    .catch(function () { /* offline or rate-limited — fallback already applied */ });
})();
