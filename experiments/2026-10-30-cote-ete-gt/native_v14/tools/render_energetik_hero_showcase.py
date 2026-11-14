#!/usr/bin/env python3
"""Software-render the exact shared Energetik facade, sign and seat geometry."""
import json, subprocess, sys
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection, Line3DCollection
from matplotlib import patheffects

ROOT=Path(__file__).resolve().parents[1]
geom=ROOT/'energetik_hero_showcase.geometry.json'
subprocess.run(['node',str(ROOT/'tools/export_energetik_hero_showcase_json.js'),str(geom)],check=True)
data=json.loads(geom.read_text());objects=data['objects']
COLORS={'concrete':'#706d62','stone':'#686052','rust':'#70452e','metal':'#566064','glass':'#55cbed','neon':'#23d9ff','seatVinylBrown':'#704632','seatSupportPaint':'#527d70','seatSteel':'#515b60','seatWood':'#64482f','seatFabric':'#57352d','seatTrim':'#542b35','wood':'#60452d'}

def faces(o):
 a,b,c=o['x0'],o['y0'],o['z0'];d,e,f=o['x1'],o['y1'],o['z1']
 p=[(a,b,c),(d,b,c),(d,e,c),(a,e,c),(a,b,f),(d,b,f),(d,e,f),(a,e,f)]
 return [[p[i] for i in q] for q in [(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(0,1,5,4),(3,7,6,2)]]

def draw_asset(ax, crop=False):
 # Draw surfaces first; modest tonal variation keeps concrete and metal facets readable.
 for o in objects:
  if o['kind']!='box':continue
  color=COLORS.get(o.get('mat'),'#706d62')
  poly=Poly3DCollection(faces(o),facecolors=color,edgecolors=(.06,.075,.083,.28),linewidths=.12)
  ax.add_collection3d(poly)
 # Tubes share exact endpoints and radius from the source helper. Glow is only a
 # screen-space halo; the narrow cyan stroke remains the individually modelled core.
 for o in objects:
  if o['kind']!='capsule':continue
  m=o.get('mat');a=o['a'];b=o['b'];r=o['r']
  if m=='neon':
   ax.plot(*zip(a,b),color='#21dfff',linewidth=8.5,alpha=.20,solid_capstyle='round')
   ax.plot(*zip(a,b),color='#7deaff',linewidth=3.5,alpha=.98,solid_capstyle='round',path_effects=[patheffects.Stroke(linewidth=4.2,foreground='#c5f8ff'),patheffects.Normal()])
  elif m=='glass':
   ax.plot(*zip(a,b),color='#5cd6fb',linewidth=5.0,alpha=.34,solid_capstyle='round')
  else:
   ax.plot(*zip(a,b),color=COLORS.get(m,'#586064'),linewidth=max(.65,r*34),alpha=.9,solid_capstyle='round')
 if crop:
  ax.set_xlim(-7.4,7.4);ax.set_ylim(9.25,11.6);ax.set_zlim(-.65,.8);ax.view_init(elev=5,azim=-88)
 else:
  ax.set_xlim(-7.8,7.8);ax.set_ylim(-.35,11.8);ax.set_zlim(-1.4,3.2);ax.view_init(elev=17,azim=-68)
 ax.set_box_aspect((15.6,12.15,4.6));ax.set_proj_type('ortho')
 ax.set_axis_off();ax.set_facecolor('#08111a');ax.grid(False)

fig=plt.figure(figsize=(18.5,10.8),dpi=120,facecolor='#08111a')
fig.text(.045,.954,'ДВОРЕЦ ЭНЕРГЕТИК',color='#e1edf2',fontsize=18,weight='bold',ha='left',va='top')
fig.text(.047,.918,'H05 · enseigne de toiture segmentée  /  façade, tubes et rangée de sièges',color='#94b7c5',fontsize=10.2,ha='left',va='top')
ax=fig.add_axes([.025,.045,.68,.84],projection='3d');draw_asset(ax,False)
detail=fig.add_axes([.705,.17,.285,.65],projection='3d');draw_asset(detail,True)
fig.text(.72,.875,'DÉTAIL DES TUBES',color='#76dfff',fontsize=11,weight='bold',ha='left')
fig.text(.72,.125,'9 circuits séparés\n517 éléments assemblés\n171 tronçons lumineux',color='#d5e4eb',fontsize=11,linespacing=1.65,ha='left',va='top')
fig.text(.045,.035,'Objets réutilisés du catalogue GitHub : baie de façade · rangée de trois sièges · enseigne « ЭНЕРГЕТИК »',color='#9aaeb8',fontsize=8.5,ha='left')
out=Path(sys.argv[1] if len(sys.argv)>1 else '/workspace/scratch/81c4a0046242/energetik-enseigne-visualisation.png')
fig.savefig(out,dpi=120,facecolor=fig.get_facecolor());print(out)
