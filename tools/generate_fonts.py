#!/usr/bin/env python3
"""Generate compact 4-bit Inter glyphs (OFL); no font library needed on device."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
out = ['#pragma once', '#include <stdint.h>',
       'struct Glyph { uint32_t offset; uint8_t w, h, advance; int8_t left, top; };',
       'struct Font { const uint8_t *pixels; const Glyph *glyphs; uint8_t first, last; };']
for name, size, weight, chars in [('Small', 11, 500, range(32,127)),
                                 ('Label', 16, 550, range(32,127)),
                                 ('Title', 21, 650, range(32,127)),
                                 ('Price', 65, 400, range(45,58))]:
    font = ImageFont.truetype(str(root/'assets/fonts/Inter.ttf'), size*3)
    font.set_variation_by_axes([14, weight])
    pixels, glyphs = [], []
    for code in chars:
        c=chr(code)
        box=font.getbbox(c, anchor='lt')
        # Common line top based on capital height; preserve natural descenders.
        box=font.getbbox(c, anchor='ls')
        x0,y0,x1,y1=box
        w,h=(x1-x0+2)//3,(y1-y0+2)//3
        if w and h:
            im=Image.new('L',(w*3,h*3))
            ImageDraw.Draw(im).text((-x0,-y0),c,font=font,fill=255,anchor='ls')
            im=im.resize((w,h),Image.Resampling.LANCZOS)
            vals=[round(v/17) for v in im.getdata()]
        else: vals=[]
        glyphs.append((len(pixels),w,h,round(font.getlength(c)/3),round(x0/3),round(y0/3)))
        if len(vals)%2: vals.append(0)
        pixels += [(vals[i]<<4)|vals[i+1] for i in range(0,len(vals),2)]
    out.append('static const uint8_t pixels'+name+'[] = {'+','.join(map(str,pixels))+'};')
    out.append('static const Glyph glyphs'+name+'[] = {'+','.join('{'+','.join(map(str,g))+'}' for g in glyphs)+'};')
    out.append('static const Font font'+name+' = {pixels'+name+', glyphs'+name+','+str(min(chars))+','+str(max(chars))+'};')
(root/'UsdMxnMonitor/Fonts.h').write_text('\n'.join(out)+'\n')
print('Generated Inter fonts.')
