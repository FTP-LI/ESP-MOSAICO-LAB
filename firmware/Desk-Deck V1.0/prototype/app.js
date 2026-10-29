"use strict";

// A local interaction model only. No WebHID, serial, host keys or device writes.
const $ = (selector) => document.querySelector(selector);
const paths = {
  copy: '<rect x="8" y="8" width="13" height="13" rx="2"/><path d="M16 8V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h3"/>',
  paste: '<rect x="7" y="2" width="10" height="4" rx="1.5"/><path d="M7 4H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2h-2M8 12h8M8 17h5"/>',
  capture: '<path d="M8 3H5a2 2 0 0 0-2 2v3M16 3h3a2 2 0 0 1 2 2v3M21 16v3a2 2 0 0 1-2 2h-3M8 21H5a2 2 0 0 1-2-2v-3"/><rect x="7" y="7" width="10" height="10" rx="2"/>',
  undo: '<path d="m8 4-5 5 5 5M3 9h10a7 7 0 0 1 0 14" transform="translate(0 -2)"/>',
  redo: '<path d="m16 4 5 5-5 5M21 9H11a7 7 0 0 0 0 14" transform="translate(0 -2)"/>',
  desktop: '<rect x="2" y="3" width="20" height="14" rx="2"/><path d="M8 22h8M12 17v5"/>',
  play: '<path d="m8 4 13 8-13 8Z"/>',
  stop: '<rect x="5" y="5" width="14" height="14" rx="2"/>',
  terminal: '<rect x="2" y="3" width="20" height="18" rx="3"/><path d="m6 8 4 4-4 4M13 16h5"/>',
  code: '<path d="m7 6-5 6 5 6M17 6l5 6-5 6M14 3l-4 18"/>',
  search: '<circle cx="10" cy="10" r="7"/><path d="m15 15 6 6"/>',
  command: '<path d="M8 8H5a3 3 0 1 1 3-3v14a3 3 0 1 1-3-3h14a3 3 0 1 1-3 3V5a3 3 0 1 1 3 3Z"/>',
  save: '<path d="M4 3h13l4 4v14H3V3ZM7 3v6h10V3M7 21v-8h10v8"/>',
  select: '<path d="M7 3H3v4M17 3h4v4M21 17v4h-4M7 21H3v-4M3 10v4M21 10v4M10 3h4M10 21h4"/><path d="m8 12 3 3 6-6"/>',
  tab: '<rect x="2" y="3" width="20" height="18" rx="2"/><path d="M2 8h20M12 11v7M8.5 14.5h7"/>',
  restore: '<path d="M8 3H3v5M3 5a10 10 0 1 1-1 9M12 7v5l4 2"/>',
  windows: '<rect x="2" y="2" width="14" height="14" rx="2"/><path d="M9 16v4a2 2 0 0 0 2 2h9a2 2 0 0 0 2-2v-9a2 2 0 0 0-2-2h-4"/>',
  previous: '<path d="M5 5v14M20 5 8 12l12 7Z"/>',
  next: '<path d="M19 5v14M4 5l12 7-12 7Z"/>',
  mute: '<path d="m11 4-6 5H2v6h3l6 5ZM16 9l6 6M22 9l-6 6"/>',
  volumeDown: '<path d="m11 4-6 5H2v6h3l6 5ZM16 12h6"/>',
  volumeUp: '<path d="m11 4-6 5H2v6h3l6 5ZM16 12h6M19 9v6"/>',
  lock: '<rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3M12 14v3"/>',
  settings: '<path d="M4 6h16M4 12h16M4 18h16"/><rect x="7" y="4" width="3" height="4" rx="1" fill="var(--bg)"/><rect x="15" y="10" width="3" height="4" rx="1" fill="var(--bg)"/><rect x="8" y="16" width="3" height="4" rx="1" fill="var(--bg)"/>',
  close: '<path d="m6 6 12 12M18 6 6 18"/>'
};
const icon = (name) => `<svg viewBox="0 0 24 24" aria-hidden="true">${paths[name] || paths.command}</svg>`;
const escapeText = (text) => String(text).replace(/[&<>"']/g, char => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[char]));
const poseNames = { upright: "正放", right: "右转 90°", inverted: "倒置 180°", left: "左转 90°", flat: "平放" };
let config, catalog, profileIndex = 0, connection = "ready", pose = "upright";
let overlayKind = null, locked = false, selectedSlot = 0, draftSettings = null;
let keyCleanups = [], overlayCleanups = [], sentCount = 0;

function feedback(message, success = false) {
  $("#action-feedback").textContent = message;
  $("#action-feedback").classList.toggle("success", success);
}

// Every press is completed on release. Dragging cancels; a hold consumes the tap.
function attachPress(element, tap, hold, duration = 600) {
  let press = null, timer = null, lastCommit = 0;
  function cancel() {
    clearTimeout(timer);
    timer = null;
    if (press && typeof press.id === "number" && element.hasPointerCapture(press.id)) element.releasePointerCapture(press.id);
    press = null;
    element.classList.remove("pressing");
  }
  function begin(id, x = 0, y = 0) {
    if (press || element.disabled) return;
    press = { id, x, y, consumed: false };
    element.classList.add("pressing");
    timer = setTimeout(() => {
      if (!press) return;
      press.consumed = true;
      element.classList.remove("pressing");
      lastCommit = performance.now();
      hold();
    }, duration);
  }
  element.addEventListener("pointerdown", event => {
    if (event.button !== 0 || !event.isPrimary) return;
    element.setPointerCapture(event.pointerId);
    begin(event.pointerId, event.clientX, event.clientY);
  });
  element.addEventListener("pointermove", event => {
    if (press?.id === event.pointerId && Math.hypot(event.clientX - press.x, event.clientY - press.y) > 12) cancel();
  });
  element.addEventListener("pointerup", event => {
    if (press?.id !== event.pointerId) return;
    const bounds = element.getBoundingClientRect();
    const valid = !press.consumed && event.clientX >= bounds.left && event.clientX <= bounds.right && event.clientY >= bounds.top && event.clientY <= bounds.bottom;
    cancel();
    if (valid) { lastCommit = performance.now(); tap(); }
  });
  element.addEventListener("pointercancel", cancel);
  element.addEventListener("lostpointercapture", cancel);
  element.addEventListener("contextmenu", event => event.preventDefault());
  element.addEventListener("keydown", event => {
    if (!["Enter", " "].includes(event.key)) return;
    event.preventDefault();
    if (!event.repeat) begin("keyboard");
  });
  element.addEventListener("keyup", event => {
    if (!["Enter", " "].includes(event.key)) return;
    event.preventDefault();
    const valid = press?.id === "keyboard" && !press.consumed;
    cancel();
    if (valid) { lastCommit = performance.now(); tap(); }
  });
  element.addEventListener("blur", cancel);
  element.addEventListener("click", event => {
    // Assistive technology may activate a button without pointer events.
    if (event.detail === 0 && !press && performance.now() - lastCommit > 150 && !element.disabled) tap();
  });
  return cancel;
}

function renderGrid() {
  keyCleanups.forEach(cancel => cancel()); keyCleanups = [];
  const profile = config.profiles[profileIndex];
  $("#screen").dataset.accent = profile.accent;
  $("#profile-title").textContent = profile.name;
  $("#profile-caption").textContent = profile.caption;
  $("#page-number").textContent = `0${profileIndex + 1} / 04`;
  $("#profile-dots").innerHTML = config.profiles.map((_, index) => `<i class="${index === profileIndex ? "active" : ""}"></i>`).join("");
  $("#key-grid").innerHTML = profile.buttons.map((button, index) => `<button class="deck-key" data-slot="${index}" aria-label="${escapeText(button.label)}：${escapeText(button.hint)}" ${connection !== "ready" || locked ? "disabled" : ""}>${icon(button.icon)}<b>${escapeText(button.label)}</b><small>${escapeText(button.hint)}</small></button>`).join("");
  document.querySelectorAll(".deck-key").forEach(element => {
    const index = Number(element.dataset.slot);
    keyCleanups.push(attachPress(element, () => sendAction(index), () => showDetail(index)));
  });
  renderEditor();
}

function sendAction(index) {
  if (connection !== "ready" || locked || overlayKind) return;
  const button = config.profiles[profileIndex].buttons[index];
  sentCount += 1;
  $("#screen").dataset.sentCount = String(sentCount);
  feedback(`已模拟发送 · ${button.label}`, true);
  const element = $(`.deck-key[data-slot="${index}"]`);
  element?.classList.add("sent");
  setTimeout(() => element?.classList.remove("sent"), 180);
}

function switchProfile(index, reason = "已切换") {
  if (locked || overlayKind) return;
  profileIndex = (index + config.profiles.length) % config.profiles.length;
  selectedSlot = 0;
  renderGrid();
  feedback(`${reason} · ${config.profiles[profileIndex].name}`);
}

function showOverlay(kind, markup) {
  keyCleanups.forEach(cancel => cancel());
  overlayCleanups.forEach(cancel => cancel()); overlayCleanups = [];
  overlayKind = kind;
  $("#overlay").className = `overlay ${kind === "locked" ? "locked-view" : ""}`;
  $("#overlay").innerHTML = markup;
  $("#overlay").hidden = false;
  $("#overlay").setAttribute("role", "dialog");
  $("#overlay").setAttribute("aria-modal", "true");
  $("#overlay").setAttribute("aria-label", { settings: "设备设置", profiles: "选择场景", detail: "操作说明", locked: "控制台已锁定" }[kind]);
  document.querySelectorAll(".screen-header,.screen-caption,#key-grid,.screen-footer").forEach(el => { el.inert = true; });
  $("#overlay button, #overlay input")?.focus({ preventScroll: true });
}

function closeOverlay() {
  overlayCleanups.forEach(cancel => cancel()); overlayCleanups = [];
  overlayKind = null;
  $("#overlay").hidden = true;
  $("#overlay").innerHTML = "";
  document.querySelectorAll(".screen-header,.screen-caption,#key-grid,.screen-footer").forEach(el => { el.inert = false; });
  applyAppearance(config.settings);
  $("#profile-picker").focus({ preventScroll: true });
}

function overlayHeader(title) {
  return `<div class="overlay-header"><h2>${title}</h2><button class="icon-button overlay-close" aria-label="关闭弹层">${icon("close")}</button></div>`;
}

function showProfiles() {
  if (locked) return;
  showOverlay("profiles", `${overlayHeader("选择场景")}<div class="profile-options">${config.profiles.map((profile, index) => `<button class="profile-option ${index === profileIndex ? "selected" : ""}" data-profile="${index}"><b>${escapeText(profile.name)}</b><small>${poseNames[profile.orientation]} · 6 个按键</small></button>`).join("")}</div><p style="margin:16px 0 0">也可以左右滑动或翻转设备。</p>`);
  $(".overlay-close").onclick = closeOverlay;
  document.querySelectorAll(".profile-option").forEach(button => button.onclick = () => { closeOverlay(); switchProfile(Number(button.dataset.profile)); });
}

function showDetail(index) {
  if (locked || overlayKind || connection !== "ready") return;
  const button = config.profiles[profileIndex].buttons[index];
  showOverlay("detail", `${overlayHeader("操作说明")}<div class="detail-hero">${icon(button.icon)}<h3>${escapeText(button.label)}</h3><code>${escapeText(button.hint)}</code></div><p class="detail-caption">${button.action.type === "consumer" ? "发送媒体控制按键，实际状态以电脑为准。" : "发送到电脑当前正在使用的软件。"}<br>长按仅查看，不会触发操作。</p><div class="bottom-actions"><button class="secondary" id="detail-edit">编辑此按键</button><button class="primary" id="detail-close">知道了</button></div>`);
  $(".overlay-close").onclick = closeOverlay;
  $("#detail-close").onclick = closeOverlay;
  $("#detail-edit").onclick = () => { closeOverlay(); selectedSlot = index; renderEditor(); $("#edit-label").focus(); };
}

function applyAppearance(settings) {
  $("#screen").dataset.theme = settings.theme;
  // Approximate brightness locally; this does not represent real backlight PWM.
  $("#screen").style.setProperty("--preview-brightness", String(.65 + settings.brightness * .0035));
  document.querySelectorAll(".segmented button").forEach(button => button.setAttribute("aria-pressed", String(button.dataset.theme === settings.theme)));
}

function showSettings() {
  if (locked) return;
  draftSettings = structuredClone(config.settings);
  showOverlay("settings", `${overlayHeader("设备设置")}<div class="setting-slider"><label for="brightness">屏幕亮度 <span id="brightness-value">${draftSettings.brightness}%</span></label><input id="brightness" type="range" min="10" max="100" step="5" value="${draftSettings.brightness}"></div><label class="setting-toggle" for="haptics">轻触振动<input id="haptics" type="checkbox" ${draftSettings.haptics ? "checked" : ""}></label><label class="setting-toggle" for="flip-profiles">翻转切换场景<input id="flip-profiles" type="checkbox" ${draftSettings.flipProfiles ? "checked" : ""}></label><p class="settings-note">亮度可即时预览，返回会撤销。<br>此页翻转只改变朝向，不切换场景。</p><div class="bottom-actions"><button class="secondary" id="cancel-settings">返回</button><button class="primary" id="save-settings">保存设置</button></div>`);
  $("#brightness").oninput = event => { draftSettings.brightness = Number(event.target.value); $("#brightness-value").textContent = `${draftSettings.brightness}%`; applyAppearance(draftSettings); };
  $("#haptics").onchange = event => { draftSettings.haptics = event.target.checked; };
  $("#flip-profiles").onchange = event => { draftSettings.flipProfiles = event.target.checked; };
  const cancel = () => { draftSettings = null; closeOverlay(); feedback("已撤销设置修改"); };
  $(".overlay-close").onclick = cancel;
  $("#cancel-settings").onclick = cancel;
  $("#save-settings").onclick = () => { config.settings = structuredClone(draftSettings); draftSettings = null; closeOverlay(); feedback("设置已保存至本页"); };
}

function lock() {
  locked = true;
  renderGrid();
  showOverlay("locked", `${icon("lock")}<h2>暂歇一下</h2><p>控制台已锁定<br>移动与翻转不会发送按键</p><button class="unlock" id="unlock-button">长按 0.8 秒解锁</button><p class="lock-footnote">也可聚焦按钮，按住空格或 Enter</p>`);
  overlayCleanups.push(attachPress($("#unlock-button"), () => {}, () => { locked = false; closeOverlay(); renderGrid(); feedback("已解锁，等待你的下一次操作"); }, 800));
}

function renderEditor() {
  const buttons = config.profiles[profileIndex].buttons;
  $("#edit-slot").innerHTML = buttons.map((button, index) => `<option value="${index}">${index + 1}. ${escapeText(button.label)}</option>`).join("");
  $("#edit-slot").value = String(selectedSlot);
  const button = buttons[selectedSlot];
  $("#edit-label").value = button.label;
  $("#edit-action").innerHTML = catalog.map((action, index) => `<option value="${index}">${escapeText(action.label)} · ${escapeText(action.hint)}</option>`).join("");
  $("#edit-action").value = String(catalog.findIndex(item => JSON.stringify(item.action) === JSON.stringify(button.action)));
}

function updatePose(nextPose) {
  pose = nextPose;
  $("#pose-label").textContent = nextPose === "flat" ? "平放 · 保留之前的页面与朝向" : `${poseNames[nextPose]} · 文字已按观看方向对正`;
  document.querySelectorAll("[data-pose]").forEach(button => button.setAttribute("aria-pressed", String(button.dataset.pose === pose)));
  $("#flat-button").classList.toggle("active", nextPose === "flat");
  if (nextPose === "flat") { feedback("平放 · 保留当前场景"); return; }
  if (locked) return;
  if (overlayKind) { feedback("当前弹层保持显示 · 不切换场景"); return; }
  if (!config.settings.flipProfiles) { feedback("朝向已更新 · 自动切页已关闭"); return; }
  switchProfile(config.profiles.findIndex(profile => profile.orientation === pose), "翻转切换");
}

function bindControls() {
  const shell = $(".device-shell");
  new ResizeObserver(() => {
    const bezel = parseFloat(getComputedStyle(shell).paddingLeft);
    $("#screen").style.setProperty("--device-scale", String((shell.clientWidth - bezel * 2) / 480));
  }).observe(shell);
  $("#lock-button").innerHTML = icon("lock");
  $("#settings-button").innerHTML = icon("settings");
  $("#profile-picker").onclick = showProfiles;
  $("#settings-button").onclick = showSettings;
  $("#lock-button").onclick = lock;
  document.querySelectorAll("[data-pose]").forEach(button => button.onclick = () => updatePose(button.dataset.pose));
  $("#flat-button").onclick = () => updatePose("flat");
  $("#connection").onchange = event => {
    connection = event.target.value;
    $("#usb-badge").textContent = { ready: "USB 就绪", power: "仅供电", suspended: "电脑休眠" }[connection];
    $("#usb-badge").classList.toggle("unavailable", connection !== "ready");
    renderGrid();
    feedback({ ready: "连接恢复 · 请重新点击发送", power: "电脑尚未识别，快捷操作不可用", suspended: "电脑休眠，快捷操作已暂停" }[connection]);
  };
  document.querySelectorAll(".segmented button").forEach(button => button.onclick = () => {
    config.settings.theme = button.dataset.theme;
    if (draftSettings) draftSettings.theme = button.dataset.theme;
    applyAppearance(draftSettings || config.settings);
  });
  $("#edit-slot").onchange = event => { selectedSlot = Number(event.target.value); renderEditor(); };
  $("#cancel-edit").onclick = () => { renderEditor(); $("#editor-feedback").textContent = "未保存的编辑已撤销。"; };
  $("#key-editor").onsubmit = event => {
    event.preventDefault();
    const label = $("#edit-label").value.trim();
    if (!label) { $("#editor-feedback").textContent = "请填写按键名称。"; return; }
    const action = catalog[Number($("#edit-action").value)];
    const target = config.profiles[profileIndex].buttons[selectedSlot];
    Object.assign(target, { label, icon: action.icon, hint: action.hint, action: structuredClone(action.action) });
    renderGrid();
    $("#editor-feedback").textContent = `“${label}”已保存至本页，可导出配置。`;
  };
  $("#export-config").onclick = () => {
    const url = URL.createObjectURL(new Blob([JSON.stringify(config, null, 2) + "\n"], { type: "application/json" }));
    const anchor = document.createElement("a"); anchor.href = url; anchor.download = "desk-deck-config.json"; anchor.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    $("#editor-feedback").textContent = "已导出 JSON；这是配置草案，不是固件安装包。";
  };
  let swipe = null;
  $("#key-grid").addEventListener("pointerdown", event => { if (event.isPrimary) swipe = { id: event.pointerId, x: event.clientX, y: event.clientY }; });
  window.addEventListener("pointerup", event => {
    if (swipe?.id !== event.pointerId) return;
    const dx = event.clientX - swipe.x, dy = event.clientY - swipe.y;
    swipe = null;
    if (Math.abs(dx) > 64 && Math.abs(dx) > Math.abs(dy) * 1.6) switchProfile(profileIndex + (dx < 0 ? 1 : -1), "滑动切换");
  });
  window.addEventListener("pointercancel", () => { swipe = null; });
  window.addEventListener("blur", () => { keyCleanups.forEach(cancel => cancel()); overlayCleanups.forEach(cancel => cancel()); swipe = null; });
  document.addEventListener("keydown", event => {
    if (event.key === "Escape" && overlayKind && !locked) {
      draftSettings = null; closeOverlay(); feedback("已返回，未保存的设置已撤销");
    }
    if (event.key === "Tab" && overlayKind && $("#overlay").contains(document.activeElement)) {
      const focusable = [...$("#overlay").querySelectorAll("button,input,select")].filter(el => !el.disabled);
      const first = focusable[0], last = focusable[focusable.length - 1];
      if (event.shiftKey && document.activeElement === first) { event.preventDefault(); last.focus(); }
      else if (!event.shiftKey && document.activeElement === last) { event.preventDefault(); first.focus(); }
    }
  });
}

async function start() {
  try {
    const response = await fetch("../configs/default-profiles.json");
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    config = await response.json();
    catalog = config.profiles.flatMap(profile => structuredClone(profile.buttons));
    bindControls(); renderGrid(); applyAppearance(config.settings);
    $("#screen").dataset.sentCount = "0";
  } catch (error) {
    $("#key-grid").innerHTML = `<p class="error-message">配置加载失败。请用 tools/preview.py 启动预览，再通过 localhost 打开。<br>${escapeText(error.message)}</p>`;
  }
}
start();
