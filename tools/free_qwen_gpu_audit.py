#!/usr/bin/env python3
"""Independent, read-only C++/HLSL review by locally hosted free Qwen Coder.

Runs inside GitHub's free STANDARD Ubuntu runner for a public repository.
No external paid inference service, no API keys and no repository write token.
It does NOT claim that the model's findings are verified defects.
"""
import json
import os
import pathlib
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "independent-audit"
OUT.mkdir(exist_ok=True)
ENGINE = ROOT / "experiments/2026-10-30-cote-ete-gt/native_v14"
MODEL = os.environ.get("OLLAMA_MODEL", "qwen2.5-coder:7b")
API = "http://127.0.0.1:11434/api/generate"

def source_slice(relpath: str, first: int, last: int) -> str:
    rows = (ENGINE / relpath).read_text(encoding="utf-8").splitlines()
    return "\n".join(f"{i + 1:4d}: {rows[i]}" for i in range(first - 1, min(last, len(rows))))

common = """Tu es un auditeur technique INDEPENDANT. Vérifie le code, pas son histoire.
Signale uniquement des défauts ou optimisations que l'on peut relier à une ligne
du code. N'invente pas de benchmark, de gain de FPS ou de défaut GPU.
Évite les changements de qualité ou de résultats, y compris l'ordre des
intersections à distance égale et les dépendances de synchronisation.
Réponds EN FRANÇAIS sous forme Markdown :
- Constats classés critique / important / opportunité, avec fichier + lignes.
- Pour chacun : preuve technique, risque, correctif minimal et méthode de test.
- Points que tu ne peux pas trancher sans exécution réelle sur Intel HD 620.
- Verdict de sûreté du code tel qu'il est (ne pas inférer des mesures).
Si aucune anomalie n'est démontrée, dis-le explicitement.
"""

reviews = [
    ("01-hlsl-raytracing", "Ray tracing HLSL : BVH, intersections, texture PBR et précision",
     [("src/shaders/m3d_raytrace_compute.hlsl", 1, 300)]),
    ("02-cpp-compute", "C++ D3D11 Compute : états GPU, ressources, upload, readback, profiling",
     [("src/m3d_compute11.cpp", 1, 115),
      ("src/m3d_compute11.cpp", 195, 390),
      ("m3d_engine.h", 29, 48)]),
]
report = ["# Audit IA externe gratuit — Qwen2.5-Coder 7B",
          "",
          f"- UTC : {datetime.now(timezone.utc).isoformat()}",
          f"- Modèle : \`{MODEL}\` (Ollama local sur GitHub Actions)",
          "- Coût du modèle : gratuit ; runner GitHub standard sur dépôt public",
          "- Rapport généré sans clé API et sans permission GitHub écriture",
          "- **Attention : propositions d'une IA, à vérifier avant toute modification.**", ""]
raw = {}
for slug, goal, sections in reviews:
    source = "\n\n".join(f"FICHIER {name} lignes {start}-{end}\n" + source_slice(name, start, end)
                         for name, start, end in sections)
    # Hard ceiling: avoid exceeding 16 GB runner memory with very long contexts.
    source = source[:24000]
    prompt = common + "\nOBJECTIF : " + goal + "\n\nSOURCE NUMEROTEE :\n" + source
    payload = json.dumps({"model": MODEL, "prompt": prompt, "stream": False,
                          "options": {"temperature": 0.1, "num_ctx": 12288,
                                      "num_predict": 1000}}).encode("utf-8")
    request = urllib.request.Request(API, payload, headers={"Content-Type": "application/json"})
    print(f"Auditing {slug}: {len(prompt)} prompt characters", flush=True)
    started = time.monotonic()
    try:
        with urllib.request.urlopen(request, timeout=2100) as response:
            result = json.load(response)
        answer = result.get("response", "").strip()
        if not answer:
            raise ValueError(f"Model returned no text for {slug}")
    except (urllib.error.URLError, ValueError) as exc:
        (OUT / "ERREUR.txt").write_text(f"{slug}: {type(exc).__name__}: {exc}\n", encoding="utf-8")
        raise
    elapsed = time.monotonic() - started
    raw[slug] = {"seconds": round(elapsed, 1), "model": result.get("model"),
                 "prompt_eval_count": result.get("prompt_eval_count"),
                 "eval_count": result.get("eval_count"), "answer": answer}
    report += [f"## {goal}", "", answer, "",
               f"_Durée sur runner : {elapsed:.1f} s ; contexte effectif : "
               f"{result.get('prompt_eval_count', 'N/D')} tokens._", ""]
    (OUT / f"{slug}.md").write_text(answer + "\n", encoding="utf-8")
    print(f"Completed {slug} in {elapsed:.1f} s", flush=True)
(OUT / "RAPPORT-QWEN-CODE-REVIEW.md").write_text("\n".join(report), encoding="utf-8")
(OUT / "audit-raw.json").write_text(json.dumps(raw, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("Audit report ready", flush=True)
