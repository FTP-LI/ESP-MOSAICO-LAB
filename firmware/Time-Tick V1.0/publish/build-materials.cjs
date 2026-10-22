/* Compose editable SVG posters from unchanged, native LVGL screenshots.
 * Usage: NODE_PATH=<directory containing sharp> node publish/build-materials.cjs
 * Screenshots originate from TT_PREVIEW_PROOF_DIR, not photographed hardware.
 */
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
const source = path.join(root, 'publish/screens');
const output = path.join(root, 'docs/media');
fs.mkdirSync(output, { recursive: true });
const palette = { ink: '#223d32', muted: '#6c7a6b', green: '#527b57', gold: '#b28b46', cream: '#f3f3e9' };
const escape = value => String(value).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/"/g,'&quot;');
const text = (x,y,s,size=30,color=palette.ink,weight=400,extra='') => `<text x="${x}" y="${y}" font-family="Microsoft YaHei, sans-serif" font-size="${size}" font-weight="${weight}" fill="${color}" ${extra}>${escape(s)}</text>`;
const rect = (x,y,w,h,r,fill,extra='') => `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${r}" fill="${fill}" ${extra}/>`;
const rule = (x,y,w,color='#d1dbcb') => `<path d="M${x} ${y}h${w}" fill="none" stroke="${color}" stroke-width="2"/>`;
function chip(x,y,w,label,dark=false) {
  return rect(x,y,w,44,22,dark?'#243e3b':'#e4ebda')+text(x+w/2,y+29,label,20,dark?'#dce8d5':palette.green,500,'text-anchor="middle"');
}
let serial=0;
function screen(name,x,y,size,dark=false) {
  const id='screen'+serial++;
  const bytes=fs.readFileSync(path.join(source,name+'.png')).toString('base64');
  return `<defs><clipPath id="${id}">${rect(x,y,size,size,25,'white')}</clipPath></defs>`+
    rect(x-14,y-14,size+28,size+28,39,dark?'#365049':'#ffffff','stroke="'+(dark?'#58736a':'#d5ddcd')+'" stroke-width="2"')+
    `<image x="${x}" y="${y}" width="${size}" height="${size}" href="data:image/png;base64,${bytes}" clip-path="url(#${id})"/>`;
}
function base(index,category,dark=false) {
  const fg=dark?'#e8eddf':palette.ink;
  return `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="1600" height="900" viewBox="0 0 1600 900">`+
   `<defs><linearGradient id="bg" x2="1" y2="1"><stop stop-color="${dark?'#10272b':'#f9f7ed'}"/><stop offset="1" stop-color="${dark?'#172f2f':'#e9efdf'}"/></linearGradient></defs>`+
   rect(0,0,1600,900,0,'url(#bg)')+
   `<circle cx="1550" cy="40" r="330" fill="none" stroke="${dark?'#29413e':'#e3e9d9'}" stroke-width="2"/><circle cx="1550" cy="40" r="270" fill="none" stroke="${dark?'#29413e':'#e3e9d9'}" stroke-width="2"/>`+
   text(70,62,'TIME-TICK  /  '+category,20,dark?'#a6c5b7':palette.green,600,'letter-spacing="2"')+
   text(1530,62,String(index).padStart(2,'0')+' / 06',18,dark?'#a6c5b7':palette.muted,400,'text-anchor="end"')+
   rule(70,828,1460,dark?'#35504a':'#d2dccb')+
   text(70,866,'ESP-Mosaico · 翻转计时与天气小院',18,dark?'#b0c2b5':palette.muted)+
   text(1530,866,'共享 UI 电脑渲染 · 天气、电量为模拟数据',17,dark?'#b0c2b5':palette.muted,400,'text-anchor="end"');
}
async function save(name,svg) {
  const complete=svg+'</svg>';
  fs.writeFileSync(path.join(output,name+'.svg'),complete);
  await sharp(Buffer.from(complete)).png({compressionLevel:9}).toFile(path.join(output,name+'.png'));
  console.log(name);
}
async function main(){
  let p=base(1,'翻转，专注，陪伴');
  p+=text(70,208,'Time-Tick',100,palette.ink,600)+text(76,260,'V1.0 系列  ·  固件 0.1.1 / 开发预览',22,palette.muted);
  p+=text(70,378,'翻一面，',68,palette.ink,600)+text(70,468,'开始专注。',68,palette.ink,600);
  p+=text(74,547,'一个倒计时，一扇天气小窗，',28,palette.muted)+text(74,591,'还有陪你慢慢积累的呆猫。',28,palette.muted);
  p+=chip(72,661,168,'翻转即计时')+chip(256,661,168,'轻点控圆盘')+chip(440,661,168,'到时有声提醒');
  p+=screen('scene-time-1',712,181,290)+screen('countdown-zh',944,290,530);
  p+=text(738,128,'正放 · 时钟与天气',21,palette.green)+text(1038,251,'翻转 · 进入专注',23,palette.green,600);
  await save('01-cover',p);

  p=base(2,'操作方式');
  p+=text(70,157,'控制，就在圆盘上。',62,palette.ink,600)+text(74,213,'启动、暂停、继续、重置，保持简单。',26,palette.muted);
  const states=[['countdown-zh','01  翻转启动','右转 / 倒置 / 左转','默认 5 / 10 / 25 分钟'],['paused-zh','02  轻点暂停','暂停后保留当前剩余时长','专注的节奏，由你掌握'],['progress-zh','03  再轻点继续','长按 0.8 秒可停止并重置','重置后，轻点即可重新开始']];
  states.forEach((s,i)=>{const x=100+i*505;p+=screen(s[0],x,274,380)+text(x,710,s[1],31,palette.ink,600)+text(x,754,s[2],23,palette.muted)+text(x,789,s[3],21,palette.muted);});
  await save('02-controls',p);

  p=base(3,'时间与到时提醒');
  p+=text(70,159,'按你的节奏，设好每一面。',56,palette.ink,600);
  p+=screen('timer-settings-zh',733,271,342)+screen('alarm-preview-zh',1160,271,342);
  p+=text(737,671,'小时 · 分钟 · 秒',26,palette.ink,600)+text(1164,671,'铃声 · 音量 · 振动',26,palette.ink,600);
  p+=text(70,292,'滚动选时长',34,palette.green,600)+text(70,339,'三个方向分别设置，最长 24 小时。',25,palette.muted)+text(70,380,'不用输入一长串秒数。',25,palette.muted);
  p+=rule(70,423,570)+text(70,482,'9 种铃声，到时听得见',34,palette.green,600)+text(70,527,'音量可调，支持 3 秒试听。',25,palette.muted)+text(70,568,'可选 15 / 30 / 60 秒提醒与独立振动。',25,palette.muted);
  p+=rect(70,636,570,150,22,'#e2e9d7')+text(96,680,'响铃后第一次翻转 → 只关闭提醒',25,palette.ink,600)+text(96,725,'再次翻到计时方向 → 开始新计时',25,palette.ink,600)+text(96,762,'提醒结束，不会意外进入下一段。',21,palette.muted);
  await save('03-timing-alarm',p);

  p=base(4,'日间 / 夜间 / 自动',true);
  p+=text(70,157,'白天清楚，夜晚柔和。',62,'#edf1e3',600)+text(74,213,'主题跟随你的作息；夜间开始与日间恢复时间，都能自己设。',26,'#adc4b6');
  p+=screen('theme-night-home',85,285,420,true)+screen('theme-night-timer',565,285,420,true);
  p+=text(86,760,'夜间小院',28,'#e5ecd9',600)+text(566,760,'夜间计时',28,'#e5ecd9',600);
  p+=chip(1080,290,140,'日间',true)+chip(1240,290,140,'夜间',true)+chip(1400,290,130,'自动',true);
  p+=text(1080,410,'20:00 → 05:00',45,'#edf1e3',500)+text(1083,454,'默认夜间时段，可精确到分钟',23,'#adc4b6');
  p+=rule(1080,501,445,'#35504a')+text(1080,553,'设置、键盘和计时圆盘',28,'#e5ecd9',600)+text(1080,597,'一起切换明暗。',28,'#e5ecd9',600)+text(1080,663,'呆猫受光也随场景调整。',24,'#adc4b6')+text(1080,710,'亮度可实时预览，保存后记忆。',24,'#adc4b6');
  await save('04-theme',p);

  p=base(5,'天气小窗');
  p+=text(70,157,'晴雨流转，小院也有四时。',58,palette.ink,600)+text(74,213,'天气背景、雨雪粒子与呆猫活动，随天气和时段变化。',26,palette.muted);
  [['scene-time-1','晴 · 午后'],['scene-rain-a','雨 · 披上雨衣'],['scene-pet-0-season-3','雪 · 冬日小院'],['theme-night-home','夜 · 安静相伴']].forEach((s,i)=>{const x=70+i*385;p+=screen(s[0],x,302,305)+text(x+152.5,665,s[1],27,palette.ink,600,'text-anchor="middle"');});
  p+=rect(70,711,1460,83,20,'#e3ead9')+text(95,746,'内置 Open-Meteo 接口，无需填写个人天气密钥；支持联网估计地点、城市搜索和坐标定位。',24,palette.ink)+text(95,779,'联网定位需自行确认；天气为模型网格数据，精度取决于数据源。',21,palette.muted);
  await save('05-weather',p);

  p=base(6,'专注与陪伴');
  p+=text(70,157,'每一段专注，都留下足迹。',58,palette.ink,600)+text(74,213,'呆猫陪你积累，不给你增加照料任务。',26,palette.muted);
  p+=screen('achievements-zh',976,277,510);
  p+=text(72,331,'完成专注，解锁永久成就',37,palette.green,600)+text(74,383,'从第一段开始，到一小时、五小时的积累，',27,palette.muted)+text(74,426,'看见自己投入的时间。',27,palette.muted);
  p+=rule(74,477,780)+text(74,537,'54 帧呆猫动作',34,palette.green,600)+text(74,582,'散步、休息、互动；天气与时段影响活动。',26,palette.muted);
  p+=rect(70,638,800,145,22,'#e2e9d7')+text(98,688,'没有连续打卡惩罚，也不需要每日维护。',29,palette.ink,600)+text(98,737,'离开一会儿，再回来继续就好。',27,palette.muted);
  await save('06-companion',p);
}
main().catch(error=>{console.error(error);process.exitCode=1;});
