/**
 * Pripyat survey geometry, metres. Independent of renderer and UI toolkit.
 * WARNING: initial footprints are UNVERIFIED legacy scene coordinates.
 * Do not advertise these as historical measurements.
 * Coordinate convention: X east-ish scene axis, Y elevation, Z horizontal depth.
 * Georeferencing transform MUST be established from surveyed control points.
 */
export const METRES_PER_UNIT = 1;
export const preliminaryFootprints = Object.freeze({
  square: {status:"legacy-unverified", vertices:[[-53,-15],[53,-15],[53,61],[-53,61]]},
  energetik: {status:"legacy-unverified", vertices:[[-50.5,-43.4],[30.5,-43.4],[30.5,-18.8],[-50.5,-18.8]]},
  polissya: {status:"legacy-unverified", vertices:[[40,-42],[63,-42],[63,-20],[40,-20]]}
});
const finite=(v,name)=>{if(!Number.isFinite(v))throw new TypeError(name+" must be finite");return v};
export function point(x,y,z){return {x:finite(x,"x"),y:finite(y,"y"),z:finite(z,"z")}}
export function distance3D(a,b){return Math.hypot(finite(a.x,"x")-finite(b.x,"x"),finite(a.y,"y")-finite(b.y,"y"),finite(a.z,"z")-finite(b.z,"z"))}
export function horizontalDistance(a,b){return Math.hypot(finite(a.x,"x")-finite(b.x,"x"),finite(a.z,"z")-finite(b.z,"z"))}
export function polygonArea(vertices){if(vertices.length<3)throw new RangeError("polygon requires 3 vertices");let twice=0;for(let i=0;i<vertices.length;i++){const [x,z]=vertices[i],[nx,nz]=vertices[(i+1)%vertices.length];twice+=finite(x,"x")*finite(nz,"z")-finite(nx,"x")*finite(z,"z")}return Math.abs(twice)/2}
function onSegment(p,a,b){const cross=(p[0]-a[0])*(b[1]-a[1])-(p[1]-a[1])*(b[0]-a[0]);return Math.abs(cross)<1e-9&&p[0]>=Math.min(a[0],b[0])-1e-9&&p[0]<=Math.max(a[0],b[0])+1e-9&&p[1]>=Math.min(a[1],b[1])-1e-9&&p[1]<=Math.max(a[1],b[1])+1e-9}
function inside(p,poly){let yes=false;for(let i=0,j=poly.length-1;i<poly.length;j=i++){const a=poly[j],b=poly[i];if(onSegment(p,a,b))return true;if((a[1]>p[1])!==(b[1]>p[1])&&p[0]<(b[0]-a[0])*(p[1]-a[1])/(b[1]-a[1])+a[0])yes=!yes}return yes}
function segmentDistance(p,a,b){const dx=b[0]-a[0],dz=b[1]-a[1],len=dx*dx+dz*dz;const t=len?Math.max(0,Math.min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dz)/len)):0;return Math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dz)}
function segmentsCross(a,b,c,d){
  const orient=(p,q,r)=>(q[0]-p[0])*(r[1]-p[1])-(q[1]-p[1])*(r[0]-p[0]);
  const o1=orient(a,b,c),o2=orient(a,b,d),o3=orient(c,d,a),o4=orient(c,d,b);
  if((o1>0&&o2<0||o1<0&&o2>0)&&(o3>0&&o4<0||o3<0&&o4>0))return true;
  return (Math.abs(o1)<1e-9&&onSegment(c,a,b))||(Math.abs(o2)<1e-9&&onSegment(d,a,b))||
         (Math.abs(o3)<1e-9&&onSegment(a,c,d))||(Math.abs(o4)<1e-9&&onSegment(b,c,d));
}
export function footprintGap(a,b){if(a.length<3||b.length<3)throw new RangeError("invalid footprint");if(a.some(p=>inside(p,b))||b.some(p=>inside(p,a)))return 0;for(let i=0;i<a.length;i++)for(let j=0;j<b.length;j++)if(segmentsCross(a[i],a[(i+1)%a.length],b[j],b[(j+1)%b.length]))return 0;let min=Infinity;for(const poly of [a,b])for(const p of poly){const other=poly===a?b:a;for(let j=0;j<other.length;j++)min=Math.min(min,segmentDistance(p,other[j],other[(j+1)%other.length]))}return min}
export function measurePair(a,b){return {horizontal_m:horizontalDistance(a,b),spatial_m:distance3D(a,b),elevation_m:b.y-a.y}}
export function footprintReport(name,footprint){const xs=footprint.vertices.map(p=>p[0]),zs=footprint.vertices.map(p=>p[1]);return {name,status:footprint.status,width_m:Math.max(...xs)-Math.min(...xs),depth_m:Math.max(...zs)-Math.min(...zs),area_m2:polygonArea(footprint.vertices)}}
export function surveyReport(){const names=Object.keys(preliminaryFootprints);return {units:"metres",verified:false,buildings:names.map(n=>footprintReport(n,preliminaryFootprints[n])),gaps:names.flatMap((a,i)=>names.slice(i+1).map(b=>({a,b,gap_m:footprintGap(preliminaryFootprints[a].vertices,preliminaryFootprints[b].vertices)})))}}
