"""Vulkan command experiments. All candidates opt-in, no automatic source promotion."""
import json, math, os, pathlib, signal, statistics, subprocess, sys, time
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
base=dict(CSE_WARP_MAC_MERGE_DRAWS='1',CSE_WARP_VK_BGRA_READBACK='1',
          CSE_WARP_VK_FAST_FRAGMENT='1',M3D_V17_ANIMATE_CLOUDS='0',
          CSE_WARP_MAC_BATCH_MIP_BARRIERS='0',CSE_WARP_MAC_BATCH_UPLOADS='0',
          CSE_WARP_MAC_TRANSIENT='0',CSE_WARP_MAC_NO_TIMESTAMPS='0',
          MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS='0',MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS='1',
          MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS='1',MVK_CONFIG_PERFORMANCE_TRACKING='0')
profiles={
 'reference':{},
 '01-prefill':{'MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS':'1'},
 '01-arguments-off':{'MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS':'0'},
 '01-timestamps-off':{'CSE_WARP_MAC_NO_TIMESTAMPS':'1'},
 '02-mip-barriers':{'CSE_WARP_MAC_BATCH_MIP_BARRIERS':'1'},
 '03-batched-uploads':{'CSE_WARP_MAC_BATCH_UPLOADS':'1'},
 '03-async-submit':{'MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS':'0'},
 '04-transient':{'CSE_WARP_MAC_TRANSIENT':'1'},
}
cases={};results={}
def run(name,flags,rep,msaa=1,diagnostic=False):
    dest=out/f'msaa{msaa}'/str(rep)/name;dest.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env.update(base);env.update(flags)
    env['CSE_WARP_VK_MSAA']=str(msaa)
    env['CSE_WARP_VK_KHRONOS_CSV']=str(dest/'telemetry.csv')
    if diagnostic:
        env['MVK_CONFIG_PERFORMANCE_TRACKING']='1'
        env['MVK_CONFIG_PERFORMANCE_LOGGING_FRAME_COUNT']='12'
    (dest/'env.json').write_text(json.dumps({k:env[k] for k in base}|{'CSE_WARP_VK_MSAA':str(msaa)},indent=2))
    print('BEGIN',msaa,rep,name,flush=True);start=time.monotonic()
    with (dest/'run.log').open('w') as log:
        p=subprocess.Popen(['./v17_macos_opt_bench','benchmark-civic/scene.json',str(dest),'1920','1080'],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        try: code=p.wait(timeout=180)
        except subprocess.TimeoutExpired:
            try: os.killpg(p.pid,signal.SIGKILL)
            except ProcessLookupError: pass
            p.wait();code=124
    body=(dest/'run.log').read_text(errors='replace')
    data=json.loads((dest/'frames.json').read_text()) if code==0 and (dest/'frames.json').is_file() else []
    valid=len(data)==27 and [r['frame'] for r in data]==list(range(-3,24))
    actual=next((line for line in body.splitlines() if 'MSAA requested=' in line),'')
    key=(msaa,rep,name)
    cases[key]={'msaa':msaa,'rep':rep,'profile':name,'exit':code,'frames_valid':valid,'actual_msaa_log':actual,
                'transient_active':'transient attachment' in body and 'active=1' in '\n'.join(x for x in body.splitlines() if 'transient attachment' in x),
                'paired_uploads':body.count('paired image upload bytes='),'seconds':round(time.monotonic()-start,3)}
    if valid:results[key]=data
    (out/'cases.json').write_text(json.dumps(list(cases.values()),indent=2))
    print('END',cases[key],flush=True)
def summarize(name,msaa,refs='reference'):
    data=[];identical=0;compared=0;cold=[];phase={}
    for rep in range(2):
        a=results.get((msaa,rep,name),[]);b=results.get((msaa,rep,refs),[])
        if not a or not b:continue
        # Compare actual sample count rather than silently accepting MSAA fallback.
        def samples(k):
            log=cases[k]['actual_msaa_log']
            return log.split(' active=')[-1].split()[0] if ' active=' in log else None
        if samples((msaa,rep,name))!=str(msaa) or samples((msaa,rep,refs))!=str(msaa):continue
        lookup={r['frame']:r for r in b}
        cold.append(a[0]['wall_ms'])
        for r in a:
            if r['frame']<0:continue
            compared+=1;identical+=r['sha256']==lookup[r['frame']]['sha256'];data.append(r)
    for p in ('static','moving'):
        selected=[r for r in data if r['phase']==p]
        if selected:
            phase[p]={}
            for field in ('wall_ms','submit_ms','fence_ms','readback_cpu_ms','draws'):
                a=sorted(r[field] for r in selected)
                phase[p][field+'_median']=statistics.median(a)
                phase[p][field+'_p95']=a[math.ceil(.95*len(a))-1]
    return dict(profile=name,msaa=msaa,compared=compared,identical=identical,cold_ms_median=statistics.median(cold) if cold else None,phase=phase)
def qualifies(c,r):
    if c['compared']!=48 or c['identical']!=48:return False
    if not c['phase'] or not r['phase']:return False
    return all(c['phase'][p]['wall_ms_median']<=.98*r['phase'][p]['wall_ms_median'] and
               c['phase'][p]['wall_ms_p95']<=1.05*r['phase'][p]['wall_ms_p95'] for p in ('static','moving'))
for rep in range(2):
    for name in (list(profiles) if rep==0 else list(reversed(profiles))):
        run(name,profiles[name],rep)
# Explicit MSAA4 tests: tile-local depth and MSAA color are both candidates.
for rep in range(2):
    for name in (['reference','04-transient'] if rep==0 else ['04-transient','reference']):
        run(name,profiles[name],rep,4)
summaries=[summarize(name,1) for name in profiles]+[summarize(name,4) for name in ('reference','04-transient')]
ref=summaries[0]
selected=[r['profile'] for r in summaries if r['msaa']==1 and r['profile']!='reference' and qualifies(r,ref)]
# Upload/mip batching target cold start; only include if actual cold median improves and steady performance is not degraded.
for name in ('02-mip-barriers','03-batched-uploads'):
    c=next(r for r in summaries if r['profile']==name and r['msaa']==1)
    if c['compared']==48 and c['identical']==48 and c['cold_ms_median'] is not None and ref['cold_ms_median'] is not None:
        if c['cold_ms_median']<.95*ref['cold_ms_median'] and all(c['phase'][p]['wall_ms_median']<=1.05*ref['phase'][p]['wall_ms_median'] for p in ('static','moving')) and name not in selected:
            selected.append(name)
flags={}
for name in selected:flags.update(profiles[name])
# A combined candidate is actually executed, with a fresh paired reference.
for rep in range(2):
    for name in (['reference-combined','05-combined'] if rep==0 else ['05-combined','reference-combined']):
        run(name,flags if name=='05-combined' else {},rep)
combined=summarize('05-combined',1,'reference-combined')
ref2=summarize('reference-combined',1,'reference-combined')
summaries.extend([ref2,combined])
# Driver performance logging is a separate diagnostic, not used in benchmark statistics.
run('driver-telemetry',{},0,diagnostic=True)
reference_ok=ref['compared']==48 and ref2['compared']==48
complete=len(cases)==25 and reference_ok
report={'complete':complete,'all_processes_success':all(c['exit']==0 and c['frames_valid'] for c in cases.values()),
        'selected_for_combination':selected,'combined_flags':flags,'combined_exact_and_faster':qualifies(combined,ref2),
        'summary':summaries,'cases':list(cases.values()),
        'note':'Candidate failures are retained and are not promoted. No source defaults changed. No asynchronous frame pipeline or native WSI tested.'}
(out/'summary.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2),flush=True)
if not complete:sys.exit(2)
