/* SoifI18n: local translations, no external server.
   Add a language: create lang/<code>.js that calls SoifI18n.register('<code>', {...}),
   then add the code to AVAILABLE below. */
(function () {
  var CODES = { persian:'fa', arabic:'ar', hebrew:'he', urdu:'ur', kurdish_sorani:'ckb', spanish:'es',
    french:'fr', deutsch:'de', italian:'it', portuguese:'pt', russian:'ru', chinese_simplified:'zh',
    japanese:'ja', korean:'ko', hindi:'hi', bengali:'bn', tamil:'ta', thai:'th', indonesian:'id',
    turkish:'tr', swahili:'sw' };
 var AVAILABLE = {
    fa: true, ar: true, he: true, ur: true, ckb: true,
    es: true, fr: true, de: true, it: true, pt: true,
    ru: true, zh: true, ja: true, ko: true, hi: true,
    bn: true, ta: true, th: true, id: true, tr: true, sw: true,
    persian: true, arabic: true, hebrew: true, urdu: true, kurdish_sorani: true,
    spanish: true, french: true, deutsch: true, italian: true, portuguese: true,
    russian: true, chinese_simplified: true, japanese: true, korean: true,
    hindi: true, bengali: true, tamil: true, thai: true, indonesian: true,
    turkish: true, swahili: true
  };            // <-- add new language codes here
  var SKIP = { SCRIPT:1, STYLE:1, PRE:1, CODE:1, NOSCRIPT:1, SVG:1, TEXTAREA:1 };
  var ATTRS = ['alt', 'title', 'aria-label', 'placeholder'];
  var dicts = {}, origText = new WeakMap(), origAttr = new WeakMap(), origTitle = null;

  function norm(s) { return s.replace(/\s+/g, ' ').trim(); }
  function skipped(el) {
    for (; el && el.nodeType === 1; el = el.parentNode) {
      if (SKIP[String(el.tagName).toUpperCase()]) return true;
      if (el.classList && el.classList.contains('ignore')) return true;
    }
    return false;
  }
  function codeOf(name) { return CODES[name] || name; }

  function run(dict) {
    var w = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null), n, list = [];
    while ((n = w.nextNode())) list.push(n);
    list.forEach(function (t) {
      if (skipped(t.parentNode)) return;
      var o = origText.has(t) ? origText.get(t) : t.nodeValue;
      origText.set(t, o);
      var k = norm(o); if (!k) return;
      var tr = dict && dict[k];
      if (tr) { var m = o.match(/^(\s*)[\s\S]*?(\s*)$/); t.nodeValue = m[1] + tr + m[2]; }
      else t.nodeValue = o;
    });
    var els = document.querySelectorAll('[alt],[title],[aria-label],[placeholder]');
    Array.prototype.forEach.call(els, function (el) {
      if (skipped(el)) return;
      var saved = origAttr.get(el) || {};
      ATTRS.forEach(function (a) {
        if (!el.hasAttribute(a)) return;
        if (!(a in saved)) saved[a] = el.getAttribute(a);
        var tr = dict && dict[norm(saved[a])];
        el.setAttribute(a, tr || saved[a]);
      });
      origAttr.set(el, saved);
    });
    if (origTitle === null) origTitle = document.title;
    var tt = dict && dict[norm(origTitle)];
    document.title = tt || origTitle;
  }

  window.SoifI18n = {
    register: function (code, dict) { dicts[code] = dict; },
    has: function (name) { return !!AVAILABLE[codeOf(name)]; },
    apply: function (name, cb) {
      var code = codeOf(name);
      if (dicts[code]) { run(dicts[code]); cb(true); return; }
      var s = document.createElement('script');
      s.src = 'lang/' + code + '.js';
      s.onload = function () { if (dicts[code]) { run(dicts[code]); cb(true); } else cb(false); };
      s.onerror = function () { cb(false); };
      document.head.appendChild(s);
    },
    restore: function () { run(null); },
    // In the browser console: SoifI18n.missing('persian')  -> list of strings still untranslated
    missing: function (name) {
      var d = dicts[codeOf(name)] || {}, out = [], seen = {};
      var w = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null), n;
      while ((n = w.nextNode())) {
        if (skipped(n.parentNode)) continue;
        var k = norm(origText.has(n) ? origText.get(n) : n.nodeValue);
        if (k && /[A-Za-z]{2,}/.test(k) && !d[k] && !seen[k]) { seen[k] = 1; out.push(k); }
      }
      console.log(out.length + ' missing'); console.log(JSON.stringify(out, null, 1));
      return out;
    }
  };
})();
