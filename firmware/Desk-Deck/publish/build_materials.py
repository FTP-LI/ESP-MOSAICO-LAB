"""Compose publication panels from original, unretouched device screenshots."""
from pathlib import Path
import hashlib
import json
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'media'
OUT.mkdir(exist_ok=True)
BG = '#111c19'
INK = '#ecf4ed'
MUTED = '#afc4b7'
ACCENTS = ['#b9edc3', '#a5cbfa', '#f1bd98', '#c7b7f0']
FONT = Path('C:/Windows/Fonts/msyh.ttc')
BOLD = Path('C:/Windows/Fonts/msyhbd.ttc')
def font(size, bold=False):
    return ImageFont.truetype(str(BOLD if bold else FONT), size)
def text(draw, xy, value, size=28, color=INK, bold=False):
    draw.text(xy, value, font=font(size,bold), fill=color)
def base(index, eyebrow, title, subtitle):
    image = Image.new('RGB',(1600,900),BG)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((64,60,70,188),3,fill=ACCENTS[index % 4])
    text(draw,(92,52),eyebrow,22,ACCENTS[index % 4],True)
    text(draw,(90,92),title,54,INK,True)
    text(draw,(92,178),subtitle,26,MUTED)
    draw.line((64,828,1536,828),fill='#35483d',width=1)
    text(draw,(64,849),'Desk-Deck  /  ESP-Mosaico',20,MUTED)
    text(draw,(990,849),'0.1.0-dev 开发预览  ·  真实设备截图',20,MUTED)
    return image,draw
def screen(image,profile,x,y,size):
    draw=ImageDraw.Draw(image)
    draw.rounded_rectangle((x-10,y-10,x+size+10,y+size+10),26,fill='#33443a')
    capture=Image.open(ROOT / 'screens' / f'profile-{profile}.png').convert('RGB')
    assert capture.size==(480,480)
    image.paste(capture.resize((size,size),Image.Resampling.LANCZOS),(x,y))
def save(image,name):
    image.save(OUT/name,optimize=True)

im,d=base(0,'桌面快捷控制台','把常用操作，放到手边','USB 连接 Windows  ·  六键触控  ·  翻转切换场景')
screen(im,0,947,267,480)
text(d,(92,290),'轻点一下，少记一组快捷键',38,INK,True)
for n,(head,desc) in enumerate([
    ('四套工作场景','常用、开发、办公、媒体'),
    ('翻转即可切换','屏幕与场景一起转，平放保留方向'),
    ('接上电脑就绪','运行不需要 Wi-Fi 或电脑助手')]):
    y=390+n*118
    d.ellipse((94,y+8,104,y+18),fill=ACCENTS[0])
    text(d,(128,y),head,29,INK,True)
    text(d,(128,y+45),desc,25,MUTED)
save(im,'01-cover.png')

im,d=base(1,'四个方向，四种用途','从常用操作，到专注工作','轻点按钮发送一次；翻转、滑动或底部标签切换场景')
labels=[('常用','正放','复制 / 粘贴 / 截图'),('开发','右转','调试 / 终端 / 格式化'),('办公','倒置','保存 / 标签 / 窗口'),('媒体','左转','播放 / 切歌 / 音量')]
for n,(label,pose,desc) in enumerate(labels):
    x=78+n*374
    screen(im,n,x,300,320)
    text(d,(x,653),f'{label}  ·  {pose}',31,ACCENTS[n],True)
    text(d,(x,708),desc,23,MUTED)
text(d,(92,779),'开发页采用 VS Code 默认键位；媒体动作取决于主机与播放器。',22,MUTED)
save(im,'02-profiles.png')

im,d=base(2,'触控与姿态','顺手操作，也留好误触边界','手指轻点做事，长按查看说明；切换场景不发送快捷键')
screen(im,2,89,280,480)
for n,(number,head,desc) in enumerate([
    ('01','轻点发送，长按查看','约 0.6 秒进入说明；滑出按钮取消本次点击'),
    ('02','翻转跟随，平放保持','稳定约 0.4 秒后切换，设置页同步旋转'),
    ('03','亮度即时预览','返回撤销亮度，保存后保留偏好'),
    ('04','需要时锁定','临时离开点锁定，回来长按解锁')]):
    y=282+n*125
    text(d,(648,y),number,24,ACCENTS[2],True)
    text(d,(715,y-6),head,31,INK,True)
    text(d,(715,y+46),desc,23,MUTED)
save(im,'03-controls.png')

im,d=base(3,'开始使用','一根数据线，连上你的工作流','ESP32-S31 版 ESP-Mosaico  ·  Windows  ·  Vibe Mode 0.2 / ABI 2')
for n,(head,lines) in enumerate([
    ('安装预览版',['按住 AI 键开机进入 Vibe Mode','使用官方入口安装 .irisfw']),
    ('等待 USB 就绪',['用数据线连接 Windows','屏幕显示「USB 就绪」后操作']),
    ('选中窗口，轻点操作',['先在记事本测试复制和粘贴','再切换到你要使用的软件'])]):
    x=76+n*504
    d.rounded_rectangle((x,285,x+472,539),20,fill='#22312a')
    text(d,(x+26,307),f'0{n+1}',28,ACCENTS[n],True)
    text(d,(x+26,357),head,32,INK,True)
    for k,line in enumerate(lines): text(d,(x+26,423+k*43),line,23,MUTED)
text(d,(92,590),'本版已实测',28,INK,True)
text(d,(92,640),'复制 / 粘贴、正放 / 倒置与设置触控；安装、枚举、恢复和重启。',25,MUTED)
text(d,(92,705),'开发预览范围',28,ACCENTS[3],True)
text(d,(92,753),'四套固定按键；网页配置未接入，其他动作与长期稳定性仍待完整验收。',24,MUTED)
save(im,'04-install.png')

files=sorted((ROOT/'screens').glob('*.png'))+sorted(OUT.glob('*.png'))
manifest={'generated_by':'publish/build_materials.py','screenshot_source':'ESP-Mosaico, validated Desk-Deck 0.1.0-dev, 2026-10-25','firmware_elf_sha256':'41188647a03ed42e0d1e6cfe3b583a03e14411173cfa410024d9c712c44aa210','font':'Microsoft YaHei, raster output only; font files not redistributed','files':[{'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]}
(ROOT/'materials-index.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Created',len(files),'screenshots and publication panels')
