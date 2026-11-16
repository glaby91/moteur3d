#!/usr/bin/env python3
"""Anchor the red hammer and sickle to the V17.443 concrete socle."""
import argparse,json,math
from pathlib import Path
ROOT="Place/Monument marteau faucille"
def read(p): return json.loads(p.read_text(encoding="utf-8-sig"))
def write(p,o):
 p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(o,ensure_ascii=False,separators=(",",":"))+"\n",encoding="utf-8")
def cube():
 v=[[-.5,-.5,-.5],[.5,-.5,-.5],[-.5,.5,-.5],[.5,.5,-.5],[-.5,-.5,.5],[.5,-.5,.5],[-.5,.5,.5],[.5,.5,.5]]
 q=[[0,2,3,1],[4,5,7,6],[0,1,5,4],[2,6,7,3],[0,4,6,2],[1,3,7,5]];f=[]
 for a,b,c,d in q:f.extend([[a,b,c],[a,c,d],[c,b,a],[d,c,a]])
 return {"id":"v500_anchor_box","type":"triangle_mesh","vertices":v,"faces":f,"uv":[[0,0] for _ in v]}
def hexnut():
 v=[]
 for y in (-.5,.5):
  for i in range(6):
   a=2*math.pi*i/6;v.append([.5*math.cos(a),y,.5*math.sin(a)])
 v.extend([[0,-.5,0],[0,.5,0]]);f=[]
 for i in range(6):
  j=(i+1)%6;f.extend([[12,j,i],[13,6+i,6+j],[i,j,6+j],[i,6+j,6+i]])
 f.extend([[c,b,a] for a,b,c in f]);return {"id":"v500_anchor_hex","type":"triangle_mesh","vertices":v,"faces":f,"uv":[[0,0] for _ in v]}
def main(root):
 root=Path(root);mp=root/"scene.json";scene=read(mp);sec=scene["sectors"][0]
 geo={}
 for r in sec.get("geometry",[]):
  for m in read(root/r).get("meshes",[]):geo[m["id"]]=m
 refs=[]
 for r in sec.get("instances",[]):
  d=read(root/r)
  refs.extend((r,o) for o in d.get("instances",[]))
 plinth=next((o for _,o in refs if o.get("parent")==ROOT+"/Socle beton"),None)
 statue=[o for _,o in refs if o.get("parent")==ROOT+"/Sculpture acier rouge"]
 if not plinth or len(statue)!=4:raise RuntimeError("Expected concrete plinth and four red sculpture parts")
 pm=geo.get(plinth["geometry"])
 if not pm:raise RuntimeError("Concrete plinth mesh missing")
 top=plinth["position"][1]+max(v[1] for v in pm["vertices"])*plinth.get("size",[1,1,1])[1]
 cx,_,cz=plinth["position"];cm=plinth["material"];sm=statue[0]["material"];adds=[]
 def add(tag,g,m,p,s,parent):
  adds.append({"id":"v500_anchor_"+tag,"parent":ROOT+"/Fixations scellees dans le beton/"+parent,"geometry":g,"material":m,"position":[round(x,5) for x in p],"size":[round(x,5) for x in s],"rotationY":0})
 attached=0
 for obj in statue:
  if obj["geometry"] not in ("v6_sickle_original","v6_hammer_handle"):continue
  mesh=geo.get(obj["geometry"])
  if not mesh:raise RuntimeError("Missing monument mesh "+obj["geometry"])
  verts=mesh["vertices"];y0=min(v[1] for v in verts);low=[v for v in verts if v[1]<=y0+.025]
  lx=sum(v[0] for v in low)/len(low);lz=sum(v[2] for v in low)/len(low)
  size=obj.get("size",[1,1,1]);pos=obj["position"];a=math.radians(obj.get("rotationY",0))
  x=pos[0]+lx*size[0]*math.cos(a)+lz*size[2]*math.sin(a)
  z=pos[2]-lx*size[0]*math.sin(a)+lz*size[2]*math.cos(a);y=pos[1]+y0*size[1]
  sickle=obj["geometry"]=="v6_sickle_original";tag="sickle" if sickle else "hammer";w=.76 if sickle else .70;d=.54
  add(tag+"_core","v500_anchor_box",cm,(x,top-.19,z),(.46,.38,.44),tag+"/Noyau beton noye")
  add(tag+"_shoe","v500_anchor_box",sm,(x,top+.035,z),(w,.07,d),tag+"/Platine acier")
  gap=max(0,y-(top+.07))
  if gap>.035:add(tag+"_support","v500_anchor_box",sm,(x,top+.07+gap/2,z-.045),(.15,gap,.12),tag+"/Jambe de reprise")
  for dx in (-w*.31,w*.31):
   for dz in (-d*.30,d*.30):
    add(tag+"_rod","v500_anchor_box",sm,(x+dx,top-.145,z+dz),(.038,.38,.038),tag+"/Tige ancree")
    add(tag+"_nut","v500_anchor_hex",sm,(x+dx,top+.071,z+dz),(.105,.055,.105),tag+"/Ecrou visible")
  attached+=1
 if attached!=2:raise RuntimeError("Both lower sculpture feet must be anchored")
 removed=0
 for r in sec.get("instances",[]):
  p=root/r;doc=read(p);keep=[]
  for o in doc.get("instances",[]):
   parent=o.get("parent","").lower();x,_,z=o.get("position",[0,0,0])
   vegetation=any(k in parent for k in ("herbe","fleur","couvre-sol","grass","groundcover"))
   if vegetation and abs(x-cx)<4.2 and cz-2.4<z<cz+4.8:removed+=1
   else:keep.append(o)
  doc["instances"]=keep;write(p,doc)
 sec["geometry"].append("geometry/monument-v500-anchors.json")
 sec["instances"].append("instances/monument-v500-anchors.json")
 scene["revision"]="V17.500"
 write(root/"geometry/monument-v500-anchors.json",{"format":"m3d-geometry-v1","units":"m","meshes":[cube(),hexnut()]})
 write(root/"instances/monument-v500-anchors.json",{"format":"m3d-instances-v1","instances":adds})
 write(mp,scene)
 report={"revision":"V17.500","anchorAssemblies":2,"embeddedConcreteCores":2,"embeddedAnchorRods":8,"visibleAnchorNuts":8,"steelSupportLinks":sum(1 for o in adds if o["id"].endswith("_support")),"clearedVegetation":removed}
 write(root/"V500_MONUMENT_ANCHORS.json",report);print(json.dumps(report,ensure_ascii=False,indent=2))
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("--package",type=Path,required=True);main(p.parse_args().package)
