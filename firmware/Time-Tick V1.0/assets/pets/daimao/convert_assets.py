"""Pack original source sheets into LVGL C data. Requires Python 3 and Pillow.

This only slices, aligns, resizes and converts generated artwork. A common crop
and scale is used for all frames in each row: no per-frame recentering/jitter.
Optional --proof writes contact sheets/GIFs into ignored build-assets/.
"""
from pathlib import Path
import argparse
from PIL import Image, ImageOps, ImageFilter, ImageChops

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
OUT=ROOT/'main'/'generated'
SIZE=104


def descriptor(name,image,alpha):
    w,h=image.size
    rgba=image.convert('RGBA')
    raw=bytearray()
    for r,g,b,a in rgba.getdata():
        v=((r>>3)<<11)|((g>>2)<<5)|(b>>3); raw.extend((v&255,v>>8))
    raw.extend(rgba.getchannel('A').tobytes())
    fmt,stride='RGB565A8',w*2
    lines=[f'static const uint8_t {name}_data[] = {{']
    lines+=['    '+','.join(f'0x{x:02x}' for x in raw[i:i+24])+',' for i in range(0,len(raw),24)]
    lines+=['};',f'const lv_image_dsc_t {name} = {{',
            f'    .header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_{fmt},.w={w},.h={h},.stride={stride}}},',
            f'    .data_size=sizeof({name}_data),.data={name}_data','};']
    return '\n'.join(lines)


def cell(sheet,col,row,cols,rows):
    return sheet.crop((round(col*sheet.width/cols),round(row*sheet.height/rows),
                       round((col+1)*sheet.width/cols),round((row+1)*sheet.height/rows)))


def bounds(im):
    box=im.getchannel('A').point(lambda a:255 if a>40 else 0).getbbox()
    if box is None: raise ValueError('Empty animation cell')
    return box


def extract_cells(sheet,rows,cols=6,split_wide=False):
    """Extract whole alpha islands, including props that cross a grid boundary.

    Generated sheets do not have exact gutters. Uniform cuts can include the
    next row's ears or cut a ball in half. Assign detached props to the closest
    character; keep original grid coordinates, so animation motion is retained.
    """
    w,h=sheet.size
    pixels=sheet.getchannel('A').tobytes()
    seen=bytearray(w*h); islands=[]
    for start in range(w*h):
        if seen[start] or pixels[start]<=40: continue
        seen[start]=1; stack=[start]; points=[]
        while stack:
            p=stack.pop(); points.append(p)
            x,y=p%w,p//w
            for q in ((p-1 if x else -1),(p+1 if x+1<w else -1),
                      (p-w if y else -1),(p+w if y+1<h else -1)):
                if q>=0 and not seen[q] and pixels[q]>40:
                    seen[q]=1; stack.append(q)
        if len(points)>=16:
            cx=sum(p%w for p in points)/len(points)
            cy=sum(p//w for p in points)/len(points)
            islands.append((points,cx,cy))
    if split_wide:
        separated=[]
        for points,cx,cy in islands:
            # Some generated neighbours touch at their ear edges. Slice those
            # joined islands at the known atlas boundary, retaining all other
            # islands and their original frame coordinates.
            if max(p%w for p in points)-min(p%w for p in points)>w/cols*1.6:
                groups={}
                for p in points: groups.setdefault(min(cols-1,p%w*cols//w),[]).append(p)
                for group in groups.values():
                    separated.append((group,sum(p%w for p in group)/len(group),sum(p//w for p in group)/len(group)))
            else: separated.append((points,cx,cy))
        islands=separated
    bodies={}
    for island in islands:
        points,cx,cy=island
        key=(min(cols-1,int(cx*cols/w)),min(rows-1,int(cy*rows/h)))
        if len(points)>len(bodies.get(key,([],0,0))[0]): bodies[key]=island
    if len(bodies)!=cols*rows: raise ValueError(f'Expected {cols*rows} character silhouettes')
    masks={key:bytearray(w*h) for key in bodies}
    for points,cx,cy in islands:
        key=min(bodies,key=lambda k:(cx-bodies[k][1])**2+(cy-bodies[k][2])**2)
        for p in points: masks[key][p]=255
    result={}
    for (col,row),raw in masks.items():
        mask=Image.frombytes('L',(w,h),bytes(raw)).filter(ImageFilter.MaxFilter(7))
        isolated=sheet.copy()
        isolated.putalpha(ImageChops.multiply(sheet.getchannel('A'),mask))
        pad=32
        result[col,row]=isolated.crop((round(col*w/cols)-pad,round(row*h/rows)-pad,
                                      round((col+1)*w/cols)+pad,round((row+1)*h/rows)+pad))
    return result


def align_row(frames):
    boxes=[bounds(f) for f in frames]
    crop=(min(b[0] for b in boxes),min(b[1] for b in boxes),max(b[2] for b in boxes),max(b[3] for b in boxes))
    scale=min(96/(crop[2]-crop[0]),92/(crop[3]-crop[1]))
    width,height=round((crop[2]-crop[0])*scale),round((crop[3]-crop[1])*scale)
    result=[]
    for f in frames:
        sprite=Image.new('RGBA',(SIZE,SIZE))
        resized=f.crop(crop).resize((width,height),Image.Resampling.LANCZOS)
        sprite.alpha_composite(resized,((SIZE-width)//2,98-height)); result.append(sprite)
    if len({im.tobytes() for im in result})!=6: raise ValueError('Repeated animation frames')
    return result


def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--proof',action='store_true'); args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    sheet=Image.open(HERE/'atlas.png').convert('RGBA')
    if sheet.getchannel('A').getextrema()!=(0,255): raise ValueError('Expected transparent alpha atlas')
    cells=extract_cells(sheet,8,6,split_wide=True)
    rows=[align_row([cells[col,row] for col in range(6)]) for row in range(8)]
    # Atlas rows: idle, walk, eat, play, wash, sleep, reward, weather.
    # Runtime clip order retains the existing action enum.
    clips=[rows[i] for i in (0,1,6,5,2,3,4,7)]
    clips.append([ImageOps.mirror(im) for im in rows[1]])
    output=['/* Generated from assets/pets/daimao/atlas.png; see its README. */','#include "ui_daimao.h"']
    proof=ROOT/'build-assets'/'daimao'; proof.mkdir(parents=True,exist_ok=True)
    contact=Image.new('RGB',(SIZE*6,SIZE*9),(231,239,224))
    for clip,frames in enumerate(clips):
        for frame,im in enumerate(frames):
            output.append(descriptor(f'tt_daimao_{clip}_{frame}',im,True))
            contact.paste(im,(frame*SIZE,clip*SIZE),im)
        if args.proof:
            animation=[]
            for im in frames:
                canvas=Image.new('RGB',(SIZE,SIZE),(231,239,224)); canvas.paste(im,(0,0),im); animation.append(canvas)
            animation[0].save(proof/f'clip-{clip}.gif',save_all=True,append_images=animation[1:],duration=150,loop=0)
    output.append('const lv_image_dsc_t *const tt_art_daimao[9][6] = {')
    for clip in range(9): output.append('    {'+','.join(f'&tt_daimao_{clip}_{f}' for f in range(6))+'},')
    output.append('};')
    (OUT/'ui_daimao.c').write_text('\n\n'.join(output)+'\n',encoding='utf-8')
    (OUT/'ui_daimao.h').write_text('#pragma once\n#include "lvgl.h"\nextern const lv_image_dsc_t *const tt_art_daimao[9][6];\n',encoding='utf-8')
    contact.save(proof/'contact.png')
    print('Packed 48 unique poses plus 6 mirrored gait frames; RGB565+A8 bytes:',54*SIZE*SIZE*3)

if __name__=='__main__': main()
