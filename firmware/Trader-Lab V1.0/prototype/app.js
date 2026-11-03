"use strict";
// All fixtures are synthetic. No network, storage, financial accounts or device access.
const fixtures = [
  {name:"示例宽基 ETF", code:"DEMO-A", price:"1.286", change:"+1.26%", up:true, points:[70,73,69,64,66,58,61,50,54,48,40,44,35,39,30,25]},
  {name:"示例科技 ETF", code:"DEMO-B", price:"0.932", change:"−0.84%", up:false, points:[25,30,24,34,31,39,36,45,48,42,56,52,63,61,65,70]},
  {name:"示例红利 ETF", code:"DEMO-C", price:"1.154", change:"+0.35%", up:true, points:[64,60,63,56,59,51,54,49,52,44,48,41,44,40,42,36]}
];
const pages = ["watch","chart","market","alerts"];
const names = ["自选","图表","市场","提醒"];
const directions = ["正放","右转","倒置","左转"];
const state = {page:"watch", selected:0, online:true, alert:"armed", direction:0, flip:true, theme:"dark"};
const content = document.querySelector("#content");
const screen = document.querySelector("#screen");
const dialog = document.querySelector("#settings-dialog");
const message = text => { document.querySelector("#message").textContent = text; };
const heading = (title, subtitle) => `<div class="page-heading"><h2>${title}</h2><small>${subtitle}</small></div>`;
function render() {
  const hour = new Date().getHours();
  screen.classList.toggle("light", state.theme === "light" || (state.theme === "auto" && hour >= 7 && hour < 20));
  const quality = document.querySelector("#quality");
  quality.classList.toggle("offline", !state.online);
  quality.innerHTML = `<span class="demo">● 演示数据</span><span>${state.online ? "模拟在线 · 行情时间 14:32" : "模拟离线 · 缓存于 14:32"}</span>`;
  document.querySelectorAll("[data-page]").forEach(button => {
    if (button.dataset.page === state.page) button.setAttribute("aria-current","page");
    else button.removeAttribute("aria-current");
  });
  document.querySelectorAll("[data-direction]").forEach(button => button.classList.toggle("active", Number(button.dataset.direction) === state.direction));
  document.querySelector("#direction-label").textContent = `${directions[state.direction]} · ${names[pages.indexOf(state.page)]}`;
  document.querySelector("#network").textContent = state.online ? "模拟断网" : "恢复模拟连接";
  document.querySelector("#trigger").disabled = !state.online || state.alert !== "armed";
  const item = fixtures[state.selected];
  if (state.page === "watch") {
    content.innerHTML = heading("我的自选","3 个合成示例") + `<div class="watch-list">${fixtures.map((item,i) => `<button class="watch-row" data-select="${i}"><div><strong>${item.name}</strong><small>${item.code} · 行情演示</small></div><div><div class="price">${item.price}</div><div class="change ${item.up?"up":"down"}">${item.change}</div></div></button>`).join("")}</div><p class="tiny">${state.online?"点选标的查看图表。价格、涨跌均为合成数据。":"当前显示上次快照；到价监测已暂停。"}</p>`;
  } else if (state.page === "chart") {
    const path = item.points.map((p,i) => `${i===0?"M":"L"}${10+i*24},${p*1.4}`).join(" ");
    content.innerHTML = heading(item.name,item.code) + `<div class="chart-hero"><span class="chart-price">${item.price}</span><span class="change ${item.up?"up":"down"}">${item.change}</span></div><div class="chart-box"><svg viewBox="0 0 380 140" role="img" aria-label="合成价格趋势，不是真实行情"><path d="M0 35H380 M0 75H380 M0 115H380" stroke="var(--line)" fill="none"/><path d="${path}" fill="none" stroke="var(--accent)" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/></svg><div class="chart-labels"><span>示例日线 · 16 点</span><span>未接入真实行情</span></div></div><div class="chart-actions"><span>${state.online?"合成趋势 / 不作为分时图":"离线快照 / 未更新"}</span><button data-action="next">下一只 ›</button></div>`;
  } else if (state.page === "market") {
    content.innerHTML = heading("市场一眼","合成数据") + `<div class="indexes"><div class="index-card"><small>示例市场 A</small><strong>3206.80</strong><div class="change up">+0.62%</div></div><div class="index-card"><small>示例市场 B</small><strong>10184.25</strong><div class="change down">−0.28%</div></div></div><div class="breadth"><div class="breadth-title"><strong>市场广度</strong><span>示例家数</span></div><div class="breadth-bar"><i></i><i></i></div><div class="breadth-values"><span class="up">上涨 1,580</span><span class="down">下跌 2,470</span></div></div><p class="tiny">实机此页需另行验证指数、广度来源及数据日期。<br>不将缺失数据补为 0，不展示未经计算的综合分。</p>`;
  } else {
    const firing = state.alert === "firing";
    const inactive = state.alert === "acknowledged" || state.alert === "paused";
    const status = firing ? "已触发 · 等待确认" : state.alert === "acknowledged" ? "已确认 · 当前停用" : state.alert === "paused" ? "已暂停 · 当前停用" : state.online ? "等待有效报价跨越阈值" : "网络离线 · 监测暂停";
    const action = firing ? "确认提醒" : inactive ? "重新启用" : "暂停此规则";
    content.innerHTML = heading("价格提醒","1 条示例规则") + `<div class="alert-card ${firing?"firing":""}"><div class="alert-status">● ${status}</div><h3>示例宽基 ETF · 高于</h3><div class="alert-value">1.300</div><p>${firing?"这是模拟触发，不会播放声音或发送交易指令。":inactive?"当前规则已停用，可手动重新启用。":"规划支持铃声、振动和免打扰。<br>价格阈值编辑留待后续实现。"}</p><button data-action="alert">${action}</button></div><p class="tiny">模拟离线时禁止触发；重新连接不补发过去提醒。</p>`;
  }
}
document.querySelectorAll("[data-page]").forEach(button => button.addEventListener("click", () => {state.page=button.dataset.page;render();}));
document.querySelectorAll("[data-direction]").forEach(button => button.addEventListener("click", () => {
  state.direction=Number(button.dataset.direction);
  if (state.flip) state.page=pages[state.direction];
  message(state.flip ? "模拟翻转后页面与文字已转正；这里展示的是用户阅读方向。" : "已模拟画面转正；翻转切页关闭，当前页面保留。"); render();
}));
content.addEventListener("click", event => {
  const button=event.target.closest("button"); if(!button)return;
  if(button.dataset.select!==undefined){state.selected=Number(button.dataset.select);state.page="chart";}
  if(button.dataset.action==="next")state.selected=(state.selected+1)%fixtures.length;
  if(button.dataset.action==="alert")state.alert=state.alert==="firing"?"acknowledged":state.alert==="armed"?"paused":"armed";
  render();
});
document.querySelector("#network").addEventListener("click", () => {state.online=!state.online;message(state.online?"模拟连接恢复；保留提醒确认状态，不补发旧提醒。":"模拟离线：显示原快照与时间，暂停到价监测。");render();});
document.querySelector("#trigger").addEventListener("click", () => {if(!state.online||state.alert!=="armed")return;state.alert="firing";state.page="alerts";message("仅演示提醒到达后的画面；没有真实价格检测、铃声或振动。");render();});
document.querySelector("#settings").addEventListener("click", () => dialog.showModal());
document.querySelector("#theme").addEventListener("change", event => {state.theme=event.target.value;render();});
document.querySelector("#flip").addEventListener("change", event => {state.flip=event.target.checked;});
render();
