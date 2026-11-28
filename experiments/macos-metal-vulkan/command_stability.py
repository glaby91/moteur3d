"""Short quality gate, no performance claims or automatic promotion."""
import json, os, pathlib, re, signal, subprocess, sys
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
base={'CSE_WARP_MAC_MERGE_DRAWS':'1','CSE_WARP_VK_BGRA_READBACK':'1','CSE_WARP_VK_FAST_FRAGMENT':'1','M3D_V17_ANIMATE_CLOUDS':'0','CSE_WARP_MAC_BATCH_MIP_BARRIERS':'0','CSE_WARP_MAC_BATCH_UPLOADS':'0','CSE_WARP_MAC_TRANSIENT':'0','CSE_WARP_MAC_NO_TIMESTAMPS':'0','MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS':'0','MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS':'1','MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS':'1','MVK_CONFIG_PERFORMANCE_TRACKING':'0'}
cases=[];valid=True
for samples in (1,2,4):
    ref=None
    for name,flags in [('reference',{}),('safe_candidate',{'CSE_WARP_MAC_TRANSIENT':'1','CSE_WARP_MAC_BATCH_UPLOADS':'1','MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS':'0'})]:
        dest=out/f'msaa{samples}'/name;dest.mkdir(parents=True,exist_ok=True)
        env=os.environ.copy();env.update(base);env.update(flags);env['CSE_WARP_VK_MSAA']=str(samples)
        with (dest/'run.log').open('w') as log:
            p=subprocess.Popen(['./v17_macos_opt_bench','benchmark-civic/scene.json',str(dest),'1920','1080'],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try: code=p.wait(timeout=180)
            except subprocess.TimeoutExpired:
                try: os.killpg(p.pid,signal.SIGKILL)
                except ProcessLookupError: pass
                p.wait();code=124
        body=(dest/'run.log').read_text(errors='replace')
        rows=json.loads((dest/'frames.json').read_text()) if code==0 and (dest/'frames.json').is_file() else []
        ok=len(rows)==27 and [r['frame'] for r in rows]==list(range(-3,24))
        hashes={r['frame']:r['sha256'] for r in rows if r['frame']>=0}
        active=re.search(r'MSAA requested=\d+ active=(\d+)',body)
        sample_ok=active is not None and int(active.group(1))==samples
        if name=='reference':ref=hashes
        equal=sum(ref.get(k)==v for k,v in hashes.items())
        fallback_ok=name=='reference' or samples==1 or ('transient attachment active=0 MSAA safety fallback' in body and not any('active=1' in line for line in body.splitlines() if 'transient attachment' in line))
        case=dict(msaa=samples,profile=name,exit=code,frames=len(hashes),identical=equal,sample_ok=sample_ok,fallback_ok=fallback_ok,paired_uploads=body.count('paired image upload bytes='))
        cases.append(case);valid=valid and code==0 and ok and sample_ok and equal==24 and fallback_ok
        print(json.dumps(case),flush=True)
        (out/'summary.json').write_text(json.dumps({'complete':False,'cases':cases},indent=2))
(out/'summary.json').write_text(json.dumps({'complete':valid and len(cases)==6,'cases':cases,'note':'Stability only; no FPS promotion'},indent=2))
if not valid:sys.exit(2)
