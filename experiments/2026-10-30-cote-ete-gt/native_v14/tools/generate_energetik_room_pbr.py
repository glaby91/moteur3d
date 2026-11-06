"""Generate 2048px tiled Energetik PBR textures (procedural, photo-inspired targets).
Produces M3DPBR1 gzip mip pyramids: albedo, tangent-space normal, roughness.
No claim of pixel extraction from historical photos.
"""
import gzip
import math
import os
import random
import struct
from pathlib import Path
from PIL import Image, ImageFilter

SIZE = 2048
OUT = Path(os.environ.get("M3D_ENERGETIK_PBR_DIR", "assets/pripyat/textures/rooms"))
OUT.mkdir(parents=True, exist_ok=True)
MATERIALS = {
    "concrete": ((133,132,125), 220), "plaster": ((169,166,154), 241),
    "volcanicTuff": ((132,84,80), 231), "stone": ((131,125,113), 192),
    "brick": ((139,87,73), 227), "wood": ((109,82,58), 194),
    "metal": ((89,93,90), 170), "rust": ((119,73,51), 231),
    "seatVinylBrown": ((102,69,57), 125), "asphalt": ((83,84,80), 237),
    "debris": ((116,113,104), 238), "curtain": ((80,49,47), 243),
    "floorTerrazzo": ((139,137,128), 178),
    "floorCeramic": ((145,139,125), 175),
    "floorParquet": ((114,87,63), 193),
    "floorGym": ((120,113,99), 207),
    "floorConcreteDust": ((133,129,117), 231),
    "floorRubber": ((75,77,73), 217),
    "wallCeramic": ((167,167,156), 155),
    "wallDamp": ((122,125,112), 237),
    "paintPeeling": ((157,153,141), 231),
    "ceilingFlaked": ((171,169,156), 240),
    "doorPaint": ((105,115,103), 188),
    "windowFrame": ((87,91,86), 176),
    "stageWood": ((91,65,48), 188),
    "seatFabricWorn": ((108,72,64), 228),
    "boxingCanvas": ((126,119,105), 218),
    "boxingRope": ((147,129,110), 196),
    "paperAged": ((175,156,118), 235),
    "pipePaint": ((102,109,100), 183),
    "mosaicWall": ((143,132,116), 181),
}
def rgb565(image):
    raw = image.convert("RGB").tobytes()
    buf = bytearray(len(raw)//3*2)
    for i in range(0,len(raw),3):
        r,g,b = raw[i:i+3]
        v = ((r>>3)<<11)|((g>>2)<<5)|(b>>3)
        j=(i//3)*2
        buf[j]=v&255;buf[j+1]=v>>8
    return buf

def generate(name, base, rough, seed):
    rng = random.Random(seed)
    # Small deterministic tileable noise texture; upscale once to 2048.
    n=256
    noise=Image.new("L",(n,n))
    noise.putdata([rng.randrange(90,167) for _ in range(n*n)])
    noise=noise.resize((SIZE,SIZE),Image.Resampling.BICUBIC)
    noise=noise.filter(ImageFilter.GaussianBlur(1.0))
    import PIL.ImageChops as Chops
    baseimg=Image.new("RGB",(SIZE,SIZE),base)
    tint=Image.merge("RGB",tuple(noise.point(lambda x: max(0,min(255,round(128+(x-128)*0.27)))) for _ in range(3)))
    albedo=Chops.add(baseimg,tint,scale=1,offset=-128)
    # Derive a conservative normal field from height differences.
    h=noise.resize((512,512),Image.Resampling.BILINEAR)
    pix=h.load()
    normals=Image.new("RGB",(512,512))
    p=normals.load()
    for y in range(512):
        for x in range(512):
            dx=(pix[(x+1)%512,y]-pix[(x-1)%512,y])/255*0.42
            dy=(pix[x,(y+1)%512]-pix[x,(y-1)%512])/255*0.42
            inv=1/math.sqrt(dx*dx+dy*dy+1)
            p[x,y]=(int((.5-dx*inv*.5)*255),int((.5-dy*inv*.5)*255),int((.5+inv*.5)*255))
    normals=normals.resize((SIZE,SIZE),Image.Resampling.BILINEAR)
    roughness=Image.new("RGB",(SIZE,SIZE),(rough,rough,rough))
    dest=OUT/(name+"_2048.m3dpbr.gz")
    with gzip.open(dest,"wb",compresslevel=3) as f:
        f.write(b"M3DPBR1\\0".replace(b"\\0",b"\x00"))
        f.write(struct.pack("<II",SIZE,12))
        for level in range(12):
            side=SIZE>>level
            f.write(struct.pack("<I",side))
            for img in (albedo,normals,roughness):
                mip=img if level==0 else img.resize((side,side),Image.Resampling.BOX)
                f.write(rgb565(mip))
    print(name,round(dest.stat().st_size/1048576,2),"MiB",flush=True)

if __name__=="__main__":
    for i,(name,(color,rough)) in enumerate(MATERIALS.items()):
        generate(name,color,rough,1017+i)
