@echo off
setlocal
cd /d "%~dp0"
if not exist "moteur3d_v17_campaign.exe" (
  echo ERREUR: moteur3d_v17_campaign.exe absent. Extrais ce fichier dans le dossier du moteur.
  pause
  exit /b 2
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $branch='benchmark/luminosity-audit-v1'; $runnerFile=$null; try { $refUrl='https://api.github.com/repos/glaby91/moteur3d/git/ref/heads/'+$branch; $ref=Invoke-RestMethod -UseBasicParsing -Uri $refUrl -Headers @{ 'User-Agent'='CSE-WARP-Benchmark'; 'Accept'='application/vnd.github+json' }; $sha=$ref.object.sha; $url='https://raw.githubusercontent.com/glaby91/moteur3d/'+$sha+'/v17/tools/audit_luminosite.ps1'; $runnerFile=Join-Path $env:TEMP ('CSE-WARP-Audit-'+$sha+'.ps1'); Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $runnerFile; $env:CSE_WARP_BENCHMARK_SCRIPT_COMMIT=$sha; Write-Host ('Script benchmark GitHub, commit '+$sha) } catch { Write-Warning ('Script distant indisponible : '+$_); $runnerFile=Join-Path (Get-Location).Path 'audit_luminosite.ps1'; if(Test-Path $runnerFile){ Write-Warning 'Utilisation du script local embarque.' } }; if(!(Test-Path $runnerFile)){ Write-Error 'Script benchmark distant et copie locale absents.'; exit 2 }; & $runnerFile -Root (Get-Location).Path; exit $LASTEXITCODE"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo Audit termine. Consulte le dossier benchmark-results.
) else (
  echo Echec de l'audit, code %RC%. Le rapport de console indique la cause.
)
pause
exit /b %RC%
