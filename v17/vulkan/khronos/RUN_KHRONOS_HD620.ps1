param([ValidateRange(3,600)][int]$Seconds=20,[ValidateRange(1,8)][int]$Repetitions=3,[ValidateRange(0,120)][int]$WarmupSeconds=4)
$ErrorActionPreference='Stop'
$base=Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $base
$exe=Join-Path $base 'v17_vulkan_khronos_bench.exe'
if(!(Test-Path $exe)){throw 'Missing v17_vulkan_khronos_bench.exe: extract the full Actions artifact.'}
if(!(Test-Path (Join-Path $base 'vulkan/raster.frag.spv'))){throw 'Missing Vulkan SPIR-V shaders.'}
$culture=[Globalization.CultureInfo]::InvariantCulture
$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
$results=Join-Path $base "KHRONOS_V17_32_HD620_$stamp"
New-Item -ItemType Directory -Path $results -Force | Out-Null
"Engine: CSE WARP Vulkan V17.32 source d7c2118906214e989dd88a6077b575fa38b1f0ea; Khronos test branch." | Set-Content (Join-Path $results 'environment.txt')
"Windows: $([Environment]::OSVersion.VersionString)" | Add-Content (Join-Path $results 'environment.txt')
Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,AdapterRAM | Format-List | Out-String | Add-Content (Join-Path $results 'environment.txt')
Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | Format-List | Out-String | Add-Content (Join-Path $results 'environment.txt')
$vi=Get-Command vulkaninfo -ErrorAction SilentlyContinue
if($vi){ & $vi.Source --summary 2>&1 | Out-File (Join-Path $results 'vulkaninfo.txt') -Encoding utf8 }
$cases=@(
 @{Name='01_fixed_1light';Cubes=1;Lights=1;Motion='fixed';Images='3';Present='FIFO';Msaa='1'},
 @{Name='02_orbit_1light';Cubes=1;Lights=1;Motion='orbit';Images='3';Present='FIFO';Msaa='1'},
 @{Name='03_translate_1light';Cubes=1;Lights=1;Motion='translate';Images='3';Present='FIFO';Msaa='1'},
 @{Name='04_orbit_16lights';Cubes=1;Lights=16;Motion='orbit';Images='3';Present='FIFO';Msaa='1'},
 @{Name='05_orbit_64lights';Cubes=1;Lights=64;Motion='orbit';Images='3';Present='FIFO';Msaa='1'},
 @{Name='06_orbit_64cubes';Cubes=64;Lights=16;Motion='orbit';Images='3';Present='FIFO';Msaa='1'},
 @{Name='07_orbit_256cubes';Cubes=256;Lights=16;Motion='orbit';Images='3';Present='FIFO';Msaa='1'},
 @{Name='08_spin_scene_rebuild';Cubes=1;Lights=16;Motion='spin';Images='3';Present='FIFO';Msaa='1'},
 @{Name='09_swap2_FIFO';Cubes=1;Lights=16;Motion='orbit';Images='2';Present='FIFO';Msaa='1'},
 @{Name='10_swap3_FIFO';Cubes=1;Lights=16;Motion='orbit';Images='3';Present='FIFO';Msaa='1'},
 @{Name='11_swap3_MAILBOX';Cubes=1;Lights=16;Motion='orbit';Images='3';Present='MAILBOX';Msaa='1'},
 @{Name='12_MSAA2';Cubes=1;Lights=16;Motion='orbit';Images='3';Present='FIFO';Msaa='2'}
)
$manifest=@()
foreach($case in $cases){
 for($rep=1;$rep -le $Repetitions;$rep++){
  $name="$($case.Name)_R$rep"
  $out=Join-Path $results "$name.csv"
  $stdout=Join-Path $results "$name.stdout.log"
  $stderr=Join-Path $results "$name.stderr.log"
  $env:CSE_WARP_VK_KHRONOS_CSV=$out
  $env:CSE_WARP_VK_BENCH_SWAP_IMAGES=$case.Images
  $env:CSE_WARP_VK_BENCH_PRESENT_MODE=$case.Present
  $env:CSE_WARP_VK_MSAA=$case.Msaa
  $env:CSE_WARP_VK_FAST_FRAGMENT='0'
  Write-Host "CSE WARP Vulkan: $name"
  $argsList=@('--width','1366','--height','768','--cubes',"$($case.Cubes)",'--lights',"$($case.Lights)",'--motion',$case.Motion,'--seconds',"$Seconds")
  $rc=-999
  try{
   $proc=Start-Process -FilePath $exe -ArgumentList $argsList -WorkingDirectory $base -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
   # Never hang the complete 36-case campaign if the Intel Vulkan driver stalls.
   $limit=[Math]::Max(90,$Seconds+90)
   try {
    Wait-Process -Id $proc.Id -Timeout $limit -ErrorAction Stop
    $proc.Refresh()
    $rc=$proc.ExitCode
   } catch {
    "BENCHMARK TIMEOUT after $limit s; process stopped; continuing with next case." | Add-Content $stderr
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    $rc=124
   }
  }catch{ $_ | Out-String | Add-Content $stderr }
  $manifest += [pscustomobject]@{Run=$name;Scenario=$case.Name;Repeat=$rep;Cubes=$case.Cubes;Lights=$case.Lights;Motion=$case.Motion;RequestedImages=$case.Images;RequestedPresent=$case.Present;MSAA=$case.Msaa;DurationSeconds=$Seconds;WarmupSeconds=$WarmupSeconds;ExitCode=$rc;Csv="$name.csv"}
  Write-Host "Exit=$rc Frames CSV exists=$([bool](Test-Path $out))"
 }
}
$manifest | Export-Csv -LiteralPath (Join-Path $results 'manifest.csv') -NoTypeInformation -Encoding UTF8
$summary=@()
foreach($run in $manifest){
 $csv=Join-Path $results $run.Csv
 if(!(Test-Path $csv)){$summary += [pscustomobject]@{Run=$run.Run;Frames=0;FPS=0;P50_ms=0;P95_ms=0;P99_ms=0;GPU_ms=0;Fence_ms=0;Stutters50ms=0;Status='NO_CSV'};continue}
 $elapsed=0.0
 $measured=@()
 foreach($sample in @(Import-Csv -LiteralPath $csv)){
  $elapsed += [double]::Parse($sample.cpu_total_ms,$culture)
  if($elapsed -gt 1000*$WarmupSeconds){$measured += $sample}
 }
 if(!$measured.Count){continue}
 $pacing=@($measured | ForEach-Object { [double]::Parse($_.present_interval_ms,$culture) } | Where-Object {$_ -gt 0} | Sort-Object)
 $gpu=@($measured | Where-Object gpu_valid -eq '1' | ForEach-Object {[double]::Parse($_.gpu_total_ms,$culture)})
 $fences=@($measured | ForEach-Object {[double]::Parse($_.cpu_fence_wait_ms,$culture)})
 $mean=if($pacing.Count){($pacing | Measure-Object -Average).Average}else{0}
 $fps=if($mean -gt 0){1000/$mean}else{0}
 $summary += [pscustomobject]@{
  Run=$run.Run;Frames=$measured.Count;FPS=[Math]::Round($fps,2)
  P50_ms=$(if($pacing.Count){[Math]::Round($pacing[[int][Math]::Floor(($pacing.Count-1)*.50)],3)}else{0})
  P95_ms=$(if($pacing.Count){[Math]::Round($pacing[[int][Math]::Floor(($pacing.Count-1)*.95)],3)}else{0})
  P99_ms=$(if($pacing.Count){[Math]::Round($pacing[[int][Math]::Floor(($pacing.Count-1)*.99)],3)}else{0})
  GPU_ms=$(if($gpu.Count){[Math]::Round(($gpu | Measure-Object -Average).Average,3)}else{0})
  Fence_ms=[Math]::Round(($fences | Measure-Object -Average).Average,3)
  Stutters50ms=@($pacing | Where-Object {$_ -gt 50}).Count
  Status=$(if($run.ExitCode -eq 0){'OK'}else{'PROCESS_FAILED'})
 }
}
$summary | Export-Csv -LiteralPath (Join-Path $results 'summary.csv') -NoTypeInformation -Encoding UTF8
$summary | Format-Table -AutoSize
$zip="$results.zip"
Compress-Archive -LiteralPath $results -DestinationPath $zip -CompressionLevel Optimal -Force
Write-Host "ZIP RESULTS: $zip"
if(@($manifest | Where-Object ExitCode -ne 0).Count){Write-Warning 'Some tests failed: logs are preserved in ZIP.'}
