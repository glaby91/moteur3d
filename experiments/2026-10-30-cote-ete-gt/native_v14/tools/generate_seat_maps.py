#!/usr/bin/env python3
"""Create non-photographic PBR tiles for the Energetik seat assets."""
from pathlib import Path
import math, random
from PIL import Image, ImageFilter
from PIL import ImageDraw

OUT = Path(__file__).resolve().parents[1] / "assets/pripyat/textures/seats"
OUT.mkdir(parents=True, exist_ok=True)
SIZE = 2048

def make_maps(name, base, roughness, seed, wear="grain"):
    random.seed(seed)
    noise = Image.new("L", (SIZE, SIZE))
    noise.putdata([random.randrange(256) for _ in range(SIZE * SIZE)])
    noise = noise.filter(ImageFilter.GaussianBlur(1.2))
    broad = Image.new("L", (SIZE, SIZE))
    broad.putdata([random.randrange(256) for _ in range(SIZE * SIZE)])
    broad = broad.filter(ImageFilter.GaussianBlur(12))
    scuffs = Image.new("L", (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(scuffs)
    for _ in range(44 if wear == "vinyl" else 26):
        x, y = random.randrange(SIZE), random.randrange(SIZE)
        length = random.randrange(5, 46)
        dx, dy = random.uniform(-1, 1), random.uniform(-1, 1)
        strength = random.randrange(35, 105)
        draw.line((x, y, x+dx*length, y+dy*length), fill=strength, width=random.choice([1, 1, 2]))
    scuffs = scuffs.filter(ImageFilter.GaussianBlur(.55))
    pix_n, pix_b = list(noise.getdata()), list(broad.getdata())
    pix_s = list(scuffs.getdata())
    color = Image.new("RGB", (SIZE, SIZE)); cp=[]
    rough = Image.new("L", (SIZE, SIZE)); rp=[]
    height = Image.new("L", (SIZE, SIZE)); hp=[]
    for y in range(SIZE):
        for x in range(SIZE):
            i=y*SIZE+x
            n=(pix_n[i]-128)/128; b=(pix_b[i]-128)/128
            grain = 8*n + 14*b
            if wear == "vinyl":
                # Irregular worn scratches and fine tonal grain; no baked lighting.
                scuff = pix_s[i]
                v=grain+(scuff*.17)
                height_v=max(0,min(255,128+int(v*2)))
                rough_v=roughness+int(b*18)+int(scuff*.20)
            elif wear == "paint":
                # Speckled paint loss exposes muted brown metal at a sparse rate.
                chip = (n > .82 and b < -.02)
                v=grain-28 if chip else grain
                height_v=128 + int(v*1.8)
                rough_v=roughness+int(b*12)+(25 if chip else 0)
                if chip:
                    base_local=(82,62,43)
                else:
                    base_local=base
            elif wear == "wood":
                fiber=9*math.sin((y+seed)*.20)+4*math.sin((y+seed)*.61)
                v=grain+fiber
                height_v=128+int(v*1.7)
                rough_v=roughness+int(b*9)
                base_local=base
            else:
                v=grain; height_v=128+int(v*1.5); rough_v=roughness+int(b*7); base_local=base
            if wear != "paint" or not (wear == "paint" and n > .82 and b < -.02):
                base_local=base
            cp.append(tuple(max(0,min(255,int(c+v))) for c in base_local))
            rp.append(max(0,min(255,rough_v)))
            hp.append(max(0,min(255,height_v)))
    color.putdata(cp); rough.putdata(rp); height.putdata(hp)
    # Tangent-space normal from the procedural height field.
    hp_im=height
    normals=[]
    for y in range(SIZE):
        ym=(y-1)%SIZE; yp=(y+1)%SIZE
        for x in range(SIZE):
            dx=(hp_im.getpixel(((x+1)%SIZE,y))-hp_im.getpixel(((x-1)%SIZE,y)))*.12
            dy=(hp_im.getpixel((x,yp))-hp_im.getpixel((x,ym)))*.12
            nx,ny,nz=-dx,-dy,255
            length=math.sqrt(nx*nx+ny*ny+nz*nz)
            normals.append((int(127.5+127.5*nx/length),int(127.5+127.5*ny/length),int(127.5+127.5*nz/length)))
    normal=Image.new("RGB", (SIZE,SIZE)); normal.putdata(normals)
    mr=Image.new("RGB", (SIZE,SIZE)); mr.putdata([(0,r,0) for r in rp])
    color.save(OUT/f"{name}_basecolor.png", optimize=True)
    normal.save(OUT/f"{name}_normal.png", optimize=True)
    mr.save(OUT/f"{name}_metallic_roughness.png", optimize=True)

make_maps("seat_vinyl", (75,43,27), 151, 41, "vinyl")
make_maps("seat_support_paint", (54,103,83), 177, 23, "paint")
make_maps("seat_steel", (57,52,45), 111, 97, "paint")
make_maps("seat_wood", (73,49,31), 190, 71, "wood")
