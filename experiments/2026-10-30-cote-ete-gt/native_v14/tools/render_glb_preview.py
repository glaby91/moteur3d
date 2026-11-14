#!/usr/bin/env python3
"""Small orthographic software preview renderer for this project's textured GLB assets."""
import json, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

src=Path(sys.argv[1]); dst=Path(sys.argv[2]) if len(sys.argv)>2 else src.with_suffix('.preview.png')
data=src.read_bytes(); pos=12; chunks={}
while pos<len(data):
    size,kind=struct.unpack_from('<II',data,pos); pos+=8; chunks[kind]=data[pos:pos+size]; pos+=size
doc=json.loads(chunks[0x4e4f534a].decode('utf-8')); binary=chunks[0x004e4942]
def accessor(i):
    a=doc['accessors'][i]; v=doc['bufferViews'][a['bufferView']]
    formats={5126:np.float32,5125:np.uint32,5123:np.uint16,5121:np.uint8}
    dims={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]; off=v.get('byteOffset',0)+a.get('byteOffset',0)
    return np.frombuffer(binary,dtype=formats[a['componentType']],count=a['count']*dims,offset=off).reshape(a['count'],dims)
textures=[]
for image in doc.get('images',[]):
    v=doc['bufferViews'][image['bufferView']]; off=v.get('byteOffset',0); textures.append(np.asarray(Image.open(__import__('io').BytesIO(binary[off:off+v['byteLength']])).convert('RGB')))
W=H=1200; scale=650
camera=np.array([-1.65,1.55,-2.8],float); target=np.array([0,.42,0],float); forward=(target-camera); forward/=np.linalg.norm(forward)
right=np.cross(forward,np.array([0,1,0],float)); right/=np.linalg.norm(right); up=np.cross(right,forward)
zbuf=np.full((H,W),np.inf); out=np.zeros((H,W,3),dtype=np.uint8)
for y in range(H):
    t=y/(H-1); out[y]=[int(20+14*t),int(28+16*t),int(34+18*t)]
light=np.array([-.5,.86,-.34]); light/=np.linalg.norm(light)
for mesh in doc['meshes']:
  for prim in mesh['primitives']:
    attrs=prim['attributes']; p=accessor(attrs['POSITION']).astype(np.float64); n=accessor(attrs['NORMAL']).astype(np.float64); uv=accessor(attrs['TEXCOORD_0']).astype(np.float64); idx=accessor(prim['indices']).reshape(-1)
    rel=p-camera; pts=np.stack([rel@right,rel@up,rel@forward],axis=1); px=W*.5+pts[:,0]*scale; py=H*.60-pts[:,1]*scale
    mi=prim.get('material',0); mat=doc['materials'][mi]; pbr=mat.get('pbrMetallicRoughness',{}); texid=pbr.get('baseColorTexture',{}).get('index'); image=doc['textures'][texid]['source'] if texid is not None else None
    tex=textures[image] if image is not None else None
    base=np.array(pbr.get('baseColorFactor',[.74,.74,.74,1])[:3])
    for tri in idx.reshape(-1,3):
      xyz=np.column_stack([px[tri],py[tri]]); x0=max(0,int(np.floor(xyz[:,0].min()))); x1=min(W-1,int(np.ceil(xyz[:,0].max()))); y0=max(0,int(np.floor(xyz[:,1].min()))); y1=min(H-1,int(np.ceil(xyz[:,1].max())))
      if x1<x0 or y1<y0: continue
      a,b,c=xyz; den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
      if abs(den)<1e-9: continue
      gx,gy=np.meshgrid(np.arange(x0,x1+1),np.arange(y0,y1+1)); wa=((b[1]-c[1])*(gx-c[0])+(c[0]-b[0])*(gy-c[1]))/den; wb=((c[1]-a[1])*(gx-c[0])+(a[0]-c[0])*(gy-c[1]))/den; wc=1-wa-wb; inside=(wa>=-1e-5)&(wb>=-1e-5)&(wc>=-1e-5)
      dep=wa*pts[tri[0],2]+wb*pts[tri[1],2]+wc*pts[tri[2],2]; region=zbuf[y0:y1+1,x0:x1+1]; inside &= dep<region
      if not inside.any(): continue
      normal=wa[...,None]*n[tri[0]]+wb[...,None]*n[tri[1]]+wc[...,None]*n[tri[2]]; normal/=np.maximum(1e-9,np.linalg.norm(normal,axis=2))[...,None]; shade=.28+.72*np.maximum(0,normal@light)
      if tex is not None:
        tcoord=wa[...,None]*uv[tri[0]]+wb[...,None]*uv[tri[1]]+wc[...,None]*uv[tri[2]]; tx=(np.floor((tcoord[...,0]%1)*tex.shape[1]).astype(int))%tex.shape[1]; ty=(np.floor((1-(tcoord[...,1]%1))*tex.shape[0]).astype(int))%tex.shape[0]; color=tex[ty,tx].astype(float)/255
      else: color=np.broadcast_to(base,(y1-y0+1,x1-x0+1,3))
      color=np.clip(color*shade[...,None]*255,0,255).astype(np.uint8); region[inside]=dep[inside]; out[y0:y1+1,x0:x1+1][inside]=color[inside]
# Soft floor shadow and small presentation label are outside the mesh render.
im=Image.fromarray(out,'RGB').convert('RGBA'); shadow=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(shadow); d.ellipse((W*.27,H*.75,W*.73,H*.82),fill=(0,0,0,115)); im=Image.alpha_composite(im,shadow.filter(ImageFilter.GaussianBlur(26)))
draw=ImageDraw.Draw(im); draw.rounded_rectangle((48,48,W-48,H-48),radius=26,outline=(185,194,188,130),width=2); draw.text((76,72),'PRIPYAT  /  FAUTEUIL PUBLIC',fill=(233,225,204,255)); draw.text((76,H-104),'APERÇU RENDU À PARTIR DU MAILLAGE ET DES TEXTURES DU GLB',fill=(190,200,195,255))
im.convert('RGB').save(dst,optimize=True); print(dst)
