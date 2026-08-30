// Interactive EWI fingering diagram and collection, used by ewi_fingerings.md.
//
// The bit layout is frozen: LH1 is bit 12, descending to RHp3 as bit 0. A code
// is those 13 bits zero-padded to 15 and written big-endian as three Crockford
// base-32 characters. Shared URLs depend on this, so it can never change.

(function () {
  "use strict";

  const KEYS = ["LH1", "LHb", "LH2", "LH3", "LHp1", "LHp2",
                "RHs", "RH1", "RH2", "RH3", "RHp1", "RHp2", "RHp3"];

  // How many fingerings exist, derived from the key count rather than written
  // out, so the bound used when decoding cannot drift from the bit layout.
  const COUNT = 1 << KEYS.length;

  // Proper musical sharp (U+266F) and flat (U+266D), not "#" and "b".
  const NOTES = ["C", "C\u266F/D\u266D", "D", "D\u266F/E\u266D", "E", "F",
                 "F\u266F/G\u266D", "G", "G\u266F/A\u266D", "A", "A\u266F/B\u266D", "B"];

  const BASE32 = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

  // Adding a layout is one line here plus the SVG alongside this file. Every
  // model exposes the same 13 editable keys, so the choice only changes the
  // drawing — never the note, the code, or anything else.
  //
  // keyNotes is optional, for instruments that name a key differently or need
  // it configured a particular way. It only ever adds to the help text.
  const MODELS = [
    { id: "ewi5000", name: "Akai EWI5000", file: "ewi5000.svg" },
    { id: "ewi_solo", name: "Akai EWI Solo", file: "ewi_solo.svg" },
    { id: "nurad", name: "Berglund NuRAD", file: "nurad.svg" },
    { id: "nurad_diagram", name: "Berglund NuRAD (diagram)", file: "nurad_diagram.svg" },
    {
      id: "clarii_pro_c20",
      name: "Robkoo Clarii PRO C20",
      file: "clarii_pro_c20.svg",
      keyNotes: { RHs: "the *2 key, which must be set to Sharp" },
    },
    { id: "sylphyo", name: "Aodyo Sylphyo", file: "sylphyo.svg" },
    { id: "ap100", name: "Greaten AP100", file: "ap100.svg" },
    { id: "ap300", name: "Greaten AP300", file: "ap300.svg" },
    { id: "ap500", name: "Greaten AP500", file: "ap500.svg" },
  ];

  // Named sets are data, not code: adding one is an edit to this file alone.
  const SETS_URL = "fingering_sets.json";

  // Marks a key the instrument does not have. It stays fully interactive: the
  // diagram must be able to show any of the 8192 fingerings whatever layout is
  // chosen, and a key that could not be released would trap the user.
  const MISSING_PREFIX = "Missing-";

  // Marks a key the instrument has that this page's 13 cannot represent.
  const EXTRA_PREFIX = "Extra-";

  // Names for the extra keys we know; anything else gets the explanation
  // without a name rather than a guessed one.
  const EXTRA_NAMES = {
    FS: "F\u266F key",
    LHp3: "LHp3, programmable",
    special: "Special key, for chords and polyphony",
    mod: "Mod key, pitch bend or transpose",
    functional: "Functional key",
    "*": "* key",
  };

  // Shared collections travel in the fragment, never the query string: the
  // fragment is not sent to the server, so it dodges the ~8KB request-line cap
  // that would 414 a large collection, and collections stay out of server logs.
  //
  // The fragment is a comma-separated list of fields:
  //   v  format version
  //   f  the collection, as concatenated three-character codes
  //   k  key layout
  //   m  mirrored, on or off
  const URL_VERSION = "1";

  // Resolved against this script's own location, so it does not depend on how
  // deeply the page using it is nested.
  const HERE = document.currentScript.src;

  const host = document.getElementById("ewi-diagram");
  const noteEl = document.getElementById("ewi-note");
  const codeEl = document.getElementById("ewi-code");
  const modelEl = document.getElementById("ewi-model");
  const mirrorEl = document.getElementById("ewi-mirror");
  const setEl = document.getElementById("ewi-set");
  const copyEl = document.getElementById("ewi-copy");
  const statusEl = document.getElementById("ewi-status");
  const addEl = document.getElementById("ewi-add");
  const removeEl = document.getElementById("ewi-remove");
  const tableEl = document.getElementById("ewi-table");

  // The scratchpad: which keys are currently held down.
  const pressed = new Set();

  // The collection: a flat ordered list of fingerings. Order is significant and
  // is what a shared URL will preserve; bucketing by offset for display is a
  // stable derivation, so it round-trips.
  const collection = [];

  // Named sets, once fetched. Empty until then; the dropdown fills in.
  const sets = [];

  // Bits for the keys the chosen layout does not have, taken from the loaded
  // drawing rather than from a second list here: the Missing- groups are
  // already the source of truth, and duplicating them would be a second place
  // to get wrong.
  let absentMask = 0;

  // True while the address bar still shows the link this page was opened with.
  // Cleared on the first edit, so nobody copies a stale link from the address
  // bar believing it matches what they now see.
  let arrivedViaLink = false;

  // Set while a keystroke in the code field is driving the diagram, so that the
  // resulting render does not overwrite what is being typed.
  let typingCode = false;

  // Offset row -> its entry cell, so a re-render does not rebuild the skeleton.
  const rows = new Map();

  // ---------------------------------------------------------------- fingering

  // Semitone offset from the anchor, taken from the NuRAD source with the
  // octave and transposition terms dropped. No keys pressed gives +1 = C#.
  // The && terms are booleans in JS, so coerce them rather than relying on
  // arithmetic coercion.
  function offsetOf(n) {
    const bit = (i) => (n >> (12 - i)) & 1;

    const LH1 = bit(0), LHb = bit(1), LH2 = bit(2), LH3 = bit(3),
          LHp1 = bit(4), LHp2 = bit(5), RHs = bit(6), RH1 = bit(7),
          RH2 = bit(8), RH3 = bit(9), RHp1 = bit(10), RHp2 = bit(11),
          RHp3 = bit(12);

    return 1
      - 2 * LH1
      - (LHb && !(LH1 && LH2) ? 1 : 0)
      - LH2
      - (LH2 && LH1 ? 1 : 0)
      - 2 * LH3
      + LHp1
      - LHp2
      + (RHs && !LHp1 ? 1 : 0)
      - RH1
      - (RH1 && LH3 ? 1 : 0)
      - RH2
      - 2 * RH3
      + RHp1
      - RHp2
      - 2 * RHp3;
  }

  // Every reachable offset, high notes first. Derived from the formula rather
  // than written out, so the table cannot fall out of step with it. Rows are
  // always all shown, even the ones no manual lists a fingering for.
  const OFFSETS = [];
  {
    let lo = offsetOf(0), hi = lo;
    for (let n = 1; n < COUNT; n++) {
      const o = offsetOf(n);
      if (o < lo) lo = o;
      if (o > hi) hi = o;
    }
    for (let o = hi; o >= lo; o--) OFFSETS.push(o);
  }

  function bits() {
    return KEYS.reduce((n, name, i) => n | ((pressed.has(name) ? 1 : 0) << (12 - i)), 0);
  }

  function code(n) {
    return BASE32[(n >> 10) & 31] + BASE32[(n >> 5) & 31] + BASE32[n & 31];
  }

  // Returns null rather than a wrong answer for anything malformed, so a typo
  // in the sets file fails visibly instead of silently loading a different
  // fingering.
  function decode(text) {
    if (typeof text !== "string" || text.length !== 3) return null;

    let n = 0;
    for (const c of text.toUpperCase()) {
      const i = BASE32.indexOf(c);
      if (i < 0) return null;
      n = n * 32 + i;
    }

    return n < COUNT ? n : null;
  }

  function usesAbsentKey(n) {
    return (n & absentMask) !== 0;
  }

  function noteOf(off) {
    return NOTES[((off % 12) + 12) % 12];
  }

  function render() {
    const n = bits();
    noteEl.textContent = noteOf(offsetOf(n));

    if (!typingCode) {
      codeEl.value = code(n);
      codeEl.classList.remove("ewi-code--invalid");
    }

    syncSelection(n);
  }

  // ------------------------------------------------------------------ diagram

  codeEl.addEventListener("input", () => {
    // Codes get pasted out of forum posts with backticks and commas attached,
    // so keep only alphabet characters rather than rejecting the whole thing.
    const cleaned = Array.from(codeEl.value.toUpperCase())
      .filter((c) => BASE32.includes(c))
      .join("")
      .slice(0, 3);

    if (cleaned !== codeEl.value) codeEl.value = cleaned;

    const n = cleaned.length === 3 ? decode(cleaned) : null;

    // Fewer than three characters is unfinished, not wrong.
    codeEl.classList.toggle("ewi-code--invalid", cleaned.length === 3 && n === null);

    if (n === null) return;

    typingCode = true;
    loadFingering(n);
    typingCode = false;
  });

  // Leaving the field always restores it to the truth.
  codeEl.addEventListener("blur", () => {
    codeEl.value = code(bits());
    codeEl.classList.remove("ewi-code--invalid");
  });

  function setKeyState(group, down) {
    group.classList.toggle("ewi-key--down", down);
    group.setAttribute("aria-pressed", String(down));
  }

  function syncKeys() {
    for (const g of host.querySelectorAll(".ewi-key")) {
      setKeyState(g, pressed.has(g.dataset.key));
    }
  }

  function toggle(group) {
    const name = group.dataset.key;
    const down = !pressed.has(name);

    if (down) pressed.add(name);
    else pressed.delete(name);

    setKeyState(group, down);
    render();
  }

  function loadFingering(n) {
    pressed.clear();
    KEYS.forEach((name, i) => {
      if ((n >> (12 - i)) & 1) pressed.add(name);
    });
    syncKeys();
    render();
  }

  function describe(group, text) {
    group.setAttribute("aria-label", text);

    const title = document.createElementNS("http://www.w3.org/2000/svg", "title");
    title.textContent = text;
    group.insertBefore(title, group.firstChild);
  }

  // Illustrator escapes characters an XML id cannot hold as _xHH_, so a layer
  // named "Extra-*" reaches us as "Extra-_x2A_". Undoing that lets the names
  // above be written the way the key is labelled on the instrument, and keeps
  // them working whether the id was exported or hand-edited.
  function unescapeId(name) {
    return name.replace(/_x([0-9A-Fa-f]{2})_/g,
      (_, hex) => String.fromCharCode(parseInt(hex, 16)));
  }

  function extraDescription(id) {
    const named = EXTRA_NAMES[unescapeId(id.slice(EXTRA_PREFIX.length))];
    return (named ? named + ", " : "")
      + "not part of the fingering system this page uses";
  }

  // The key's own name, plus whatever this instrument calls it or needs of it.
  function keyDescription(key, absent, model) {
    if (absent) return key + ", not on this instrument";

    const note = model.keyNotes && model.keyNotes[key];
    return note ? key + ", " + note : key;
  }

  // Illustrator's ids are only a handoff. Strip them once they have been turned
  // into classes, so generic names like "Body" cannot collide with heading
  // anchors elsewhere on the page.
  function prepare(svg, model) {
    absentMask = 0;

    for (const g of svg.querySelectorAll("g[id]")) {
      const id = g.id;
      g.removeAttribute("id");

      const absent = id.startsWith(MISSING_PREFIX);
      const key = absent ? id.slice(MISSING_PREFIX.length) : id;

      if (KEYS.includes(key)) {
        g.classList.add("ewi-key");
        if (absent) {
          g.classList.add("ewi-key--absent");
          absentMask |= 1 << (12 - KEYS.indexOf(key));
        }
        g.dataset.key = key;
        g.setAttribute("role", "button");
        g.setAttribute("tabindex", "0");
        describe(g, keyDescription(key, absent, model));
        setKeyState(g, pressed.has(key));
      } else if (id.startsWith(EXTRA_PREFIX)) {
        g.classList.add("ewi-extra");
        describe(g, extraDescription(id));
      } else if (id === "Body") {
        g.classList.add("ewi-body");
      }
    }

    svg.addEventListener("click", (e) => {
      const g = e.target.closest(".ewi-key");
      if (g) toggle(g);
    });

    svg.addEventListener("keydown", (e) => {
      if (e.key !== "Enter" && e.key !== " ") return;
      const g = e.target.closest(".ewi-key");
      if (!g) return;
      e.preventDefault();
      toggle(g);
    });
  }

  // Counts layout loads, so that a slow response for a layout the reader has
  // already switched away from cannot arrive last and win.
  let modelToken = 0;

  // Which keys are down is model-independent, so it survives a layout change.
  function loadModel(model) {
    const token = ++modelToken;

    fetch(new URL(model.file, HERE))
      .then((r) => (r.ok ? r.text() : Promise.reject(new Error(r.status))))
      .then((text) => {
        if (token !== modelToken) return;

        const doc = new DOMParser().parseFromString(text, "image/svg+xml");
        const svg = doc.documentElement;

        // A malformed file parses into an error document rather than throwing,
        // and would otherwise be inserted into the page as one. The instanceof
        // also rejects a drawing saved without the SVG namespace, which parses
        // cleanly but renders as nothing.
        if (!(svg instanceof SVGSVGElement) || doc.querySelector("parsererror")) {
          throw new Error("not an SVG");
        }

        prepare(svg, model);
        host.replaceChildren(svg);
        renderTable();
        render();
      })
      .catch(() => {
        if (token !== modelToken) return;
        host.textContent = "Could not load the fingering diagram.";
      });
  }

  // --------------------------------------------------------------- collection

  function buildTable() {
    const frag = document.createDocumentFragment();

    for (const off of OFFSETS) {
      const label = document.createElement("div");
      label.className = "ewi-row-note";
      label.textContent = noteOf(off);

      const entries = document.createElement("div");
      entries.className = "ewi-row-entries";

      frag.append(label, entries);
      rows.set(off, entries);
    }

    tableEl.replaceChildren(frag);
  }

  // A full rebuild of the entry cells. Cheap at realistic collection sizes, and
  // it keeps one code path for add, remove and bulk replacement.
  function renderTable() {
    const buckets = new Map(OFFSETS.map((o) => [o, []]));
    for (const n of collection) buckets.get(offsetOf(n)).push(n);

    for (const off of OFFSETS) {
      const list = buckets.get(off);
      const frag = document.createDocumentFragment();

      for (const n of list) {
        const entry = document.createElement("button");
        entry.type = "button";
        entry.className = "ewi-entry";
        entry.dataset.n = n;
        entry.textContent = code(n);
        if (usesAbsentKey(n)) entry.classList.add("ewi-entry--unplayable");
        frag.appendChild(entry);
      }

      rows.get(off).replaceChildren(frag);
    }

    syncSelection(bits());
  }

  function entryFor(n) {
    return tableEl.querySelector('.ewi-entry[data-n="' + n + '"]');
  }

  // Draw the eye to one entry, which may be off-screen inside a scrolled row.
  function flash(n) {
    const el = entryFor(n);
    if (!el) return;

    el.scrollIntoView({ block: "nearest", inline: "nearest" });
    el.classList.remove("ewi-entry--flash");
    void el.offsetWidth; // restart the animation
    el.classList.add("ewi-entry--flash");
  }

  // The highlight is derived, never stored: it always marks the entry matching
  // the current fingering, and nothing at all when that fingering is not in the
  // collection. This is what stops the diagram and the table disagreeing.
  function syncSelection(n) {
    const prev = tableEl.querySelector(".ewi-entry--selected");
    if (prev) prev.classList.remove("ewi-entry--selected");

    const el = entryFor(n);
    if (el) el.classList.add("ewi-entry--selected");

    // Nothing to remove unless the current fingering is actually collected.
    removeEl.disabled = !el;
  }

  function add() {
    const n = bits();

    // Re-adding something already collected is not an error; there is simply
    // nothing to add, and the entry is already highlighted.
    if (!collection.includes(n)) {
      collection.push(n);
      clearSharedUrl();
      renderTable();
    }

    flash(n);
  }

  function remove(n) {
    const i = collection.indexOf(n);
    if (i === -1) return;

    collection.splice(i, 1);
    clearSharedUrl();
    renderTable();
  }

  function showStatus(text) {
    statusEl.textContent = text;
    statusEl.hidden = !text;
  }

  function currentModel() {
    return MODELS.find((m) => m.id === modelEl.value) || MODELS[0];
  }

  function shareUrl() {
    return location.origin + location.pathname + "#" + [
      "v=" + URL_VERSION,
      "f=" + collection.map(code).join(""),
      "k=" + modelEl.value,
      "m=" + (mirrorEl.checked ? "on" : "off"),
    ].join(",");
  }

  // Three outcomes: undefined for a fragment that is not ours to read (an
  // ordinary anchor, say), null for one that is ours but damaged, or the
  // settings. Never loads partially — a damaged link is reported rather than
  // quietly dropping the entries it could not read.
  function parseHash(hash) {
    if (!hash) return undefined;

    const fields = new Map();
    for (const part of hash.split(",")) {
      const eq = part.indexOf("=");
      if (eq === -1) return undefined;
      fields.set(part.slice(0, eq), part.slice(eq + 1));
    }

    if (!fields.has("v") && !fields.has("f")) return undefined;
    if (fields.get("v") !== URL_VERSION) return null;

    const codes = fields.get("f") || "";
    if (codes.length % 3 !== 0) return null;

    const fingerings = [];
    for (let i = 0; i < codes.length; i += 3) {
      const n = decode(codes.slice(i, i + 3));
      if (n === null) return null;
      fingerings.push(n);
    }

    // Display preferences are ignored when unrecognised rather than failing
    // the whole link: the fingerings are the payload that matters.
    const k = fields.get("k");
    const m = fields.get("m");

    return {
      fingerings,
      model: MODELS.some((x) => x.id === k) ? k : null,
      mirror: m === "on" ? true : m === "off" ? false : null,
    };
  }

  // Pasting a link into the address bar of a page that is already open changes
  // only the fragment, which reloads nothing, so this has to be handled here
  // as well as at first paint.
  // Returns true when a link was applied, which is what tells the hashchange
  // handler whether there is anything to refresh.
  function loadFromHash() {
    const shared = parseHash(location.hash.replace(/^#/, ""));
    if (shared === undefined) return false;

    if (shared === null) {
      showStatus("That shared link could not be read, so nothing was loaded.");
      return false;
    }

    showStatus("");
    collection.length = 0;
    for (const n of shared.fingerings) collection.push(n);

    // A link may carry the sharer's layout and mirror setting. Both are display
    // preferences, so one that is absent or unrecognised simply leaves the page
    // default in place rather than failing the link.
    if (shared.model !== null) modelEl.value = shared.model;
    if (shared.mirror !== null) mirrorEl.checked = shared.mirror;

    // Set after applying everything: the link and the page now agree.
    arrivedViaLink = true;
    return true;
  }

  // Once the collection no longer matches the link we arrived on, drop the
  // link. This is a single correction, not live tracking of the collection.
  function clearSharedUrl() {
    if (!arrivedViaLink) return;
    arrivedViaLink = false;
    history.replaceState(null, "", location.origin + location.pathname);
  }

  function replaceCollection(fingerings) {
    collection.length = 0;
    for (const n of fingerings) collection.push(n);
    clearSharedUrl();
    renderTable();
  }

  function decodeAll(codes) {
    const out = [];
    for (const c of codes) {
      const n = decode(c);
      if (n === null) console.warn("Ignoring unreadable fingering code:", c);
      else out.push(n);
    }
    return out;
  }

  // Every 13-bit combination. Computed rather than curated, which is why it is
  // here and not in the sets file.
  function everyFingering() {
    return Array.from({ length: COUNT }, (_, n) => n);
  }

  setEl.addEventListener("change", () => {
    const choice = setEl.value;
    setEl.value = ""; // an action, not a state: snap back to the label

    if (choice === "clear") {
      replaceCollection([]);
      return;
    }

    if (choice === "all") {
      replaceCollection(everyFingering());
      return;
    }

    const set = sets.find((s) => s.id === choice);
    if (set) replaceCollection(decodeAll(set.codes));
  });

  tableEl.addEventListener("click", (e) => {
    const entry = e.target.closest(".ewi-entry");
    if (entry) loadFingering(Number(entry.dataset.n));
  });

  // Reaching an entry by keyboard loads it, exactly as clicking it does, so the
  // diagram always shows the entry you are on. Without this, tabbing moves the
  // focus ring away from the highlight, and Delete below would then remove an
  // entry other than the one you appear to be standing on. focusin rather than
  // focus, because focus does not bubble to this delegated listener.
  tableEl.addEventListener("focusin", (e) => {
    const entry = e.target.closest(".ewi-entry");
    if (entry) loadFingering(Number(entry.dataset.n));
  });

  // Focus can only be on an entry, and an entry is only focused once it has been
  // loaded, so the current fingering is the one the cursor is standing on.
  tableEl.addEventListener("keydown", (e) => {
    if (e.key !== "Delete" && e.key !== "Backspace") return;
    const n = bits();
    if (!collection.includes(n)) return;
    e.preventDefault();
    remove(n);
  });

  let copyLabelTimer = null;

  function flashCopyLabel(text) {
    copyEl.textContent = text;
    clearTimeout(copyLabelTimer);
    copyLabelTimer = setTimeout(() => {
      copyEl.textContent = "Copy link";
    }, 1500);
  }

  copyEl.addEventListener("click", () => {
    const url = shareUrl();

    if (!navigator.clipboard) {
      flashCopyLabel("Failed");
      return;
    }

    navigator.clipboard.writeText(url)
      .then(() => flashCopyLabel("Copied"))
      .catch(() => flashCopyLabel("Failed"));
  });

  addEl.addEventListener("click", add);
  removeEl.addEventListener("click", () => remove(bits()));

  // -------------------------------------------------------------------- setup

  // The class lives on the container, not the SVG, so it survives a model
  // change replacing the drawing inside it.
  function applyMirror() {
    host.classList.toggle("ewi-diagram--mirrored", mirrorEl.checked);
  }

  for (const model of MODELS) {
    modelEl.add(new Option(model.name, model.id));
  }

  // The page stores nothing, so every visit starts from these defaults and a
  // shared link is the only way to carry a layout between visits. Both are set
  // explicitly because browsers otherwise restore form state across a reload.
  modelEl.value = MODELS[0].id;
  mirrorEl.checked = false;
  applyMirror();

  modelEl.addEventListener("change", () => {
    clearSharedUrl();
    loadModel(currentModel());
  });

  mirrorEl.addEventListener("change", () => {
    clearSharedUrl();
    applyMirror();
  });

  fetch(new URL(SETS_URL, HERE))
    .then((r) => (r.ok ? r.json() : Promise.reject(new Error(r.status))))
    .then((loaded) => {
      for (const set of loaded) {
        sets.push(set);
        setEl.add(new Option(set.name, set.id));
      }
    })
    .catch(() => {
      // No sets file, or it is malformed. Clear still works.
    });

  // replaceState does not fire hashchange, so clearing the link cannot loop.
  window.addEventListener("hashchange", () => {
    if (!loadFromHash()) return;
    applyMirror();
    renderTable();
    loadModel(currentModel());
  });

  buildTable();
  loadFromHash();
  applyMirror();
  renderTable();
  loadModel(currentModel());
})();
