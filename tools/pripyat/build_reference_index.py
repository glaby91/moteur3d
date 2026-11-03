#!/usr/bin/env python3
"""Build a metadata-only, attributed photo reference index from Wikimedia Commons."""

import argparse
import csv
import html
import os
import re
import sys
import time

try:
    import requests
except ImportError:
    sys.exit("Install dependency first: python -m pip install requests")

API = "https://commons.wikimedia.org/w/api.php"
USER_AGENT = "moteur3d-pripiat-reference-index/1.0 (research; contact via GitHub repository)"
SEARCHES = [
    "Pripyat",
    "Pripyat apartment buildings",
    "Pripyat buildings",
    "Pripyat interiors",
    "Pripyat school",
    "Pripyat hospital",
    "Pripyat amusement park",
    "Pripyat street",
    "Pripyat hotel Polissya",
    "Pripyat Palace of Culture Energetik",
    "Pripyat pool Azure",
    "Pripyat 1986",
]
FIELDS = [
    "title", "theme", "page_url", "original_url", "author",
    "license", "license_url", "source_category", "mime",
]


def clean(value):
    if not value:
        return ""
    value = re.sub(r"<[^>]+>", " ", str(value))
    return re.sub(r"\s+", " ", html.unescape(value)).strip()


def theme(title):
    s = title.casefold()
    groups = [
        ("plan_urbanisme", ("map", "plan", "mapa")),
        ("hopital_sante", ("hospital", "medical", "clinic")),
        ("ecole_education", ("school", "kindergarten", "classroom")),
        ("loisirs_culture", ("amusement", "park", "culture", "energetik", "theatre", "pool", "cafe")),
        ("interieurs", ("interior", "inside", "room", "hall", "corridor")),
        ("logements", ("apartment", "residential", "house", "building", "microdistrict")),
        ("rues_paysage", ("street", "road", "square", "panorama", "landscape", "forest")),
    ]
    for label, words in groups:
        if any(word in s for word in words):
            return label
    return "vue_generale"


def search_titles(session, query, limit):
    found = []
    continuation = {}
    while len(found) < limit:
        params = {
            "action": "query", "format": "json", "generator": "search",
            "gsrnamespace": 6, "gsrsearch": query,
            "gsrlimit": min(50, limit - len(found)),
            "prop": "imageinfo", "iiprop": "url|mime|extmetadata",
        }
        params.update(continuation)
        response = session.get(API, params=params, timeout=45)
        response.raise_for_status()
        data = response.json()
        pages = data.get("query", {}).get("pages", {})
        found.extend(pages.values())
        continuation = data.get("continue", {})
        if not continuation:
            break
        time.sleep(0.15)
    return found


def metadata(page):
    info = (page.get("imageinfo") or [{}])[0]
    ext = info.get("extmetadata", {})
    title = page.get("title", "")
    license_name = clean((ext.get("LicenseShortName") or {}).get("value"))
    author = clean((ext.get("Artist") or {}).get("value"))
    license_url = clean((ext.get("LicenseUrl") or {}).get("value"))
    mime = info.get("mime", "")
    if not mime.startswith("image/"):
        return None
    return {
        "title": title.removeprefix("File:"),
        "theme": theme(title),
        "page_url": info.get("descriptionurl", ""),
        "original_url": info.get("url", ""),
        "author": author,
        "license": license_name or "À vérifier sur la page source",
        "license_url": license_url,
        "source_category": "Wikimedia Commons",
        "mime": mime,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--limit", type=int, default=500)
    parser.add_argument("--output", default="docs/pripyat/photo-references.csv")
    args = parser.parse_args()
    if not 1 <= args.limit <= 5000:
        parser.error("--limit must be between 1 and 5000")

    session = requests.Session()
    session.headers["User-Agent"] = USER_AGENT
    seen = set()
    rows = []
    for query in SEARCHES:
        try:
            pages = search_titles(session, query, args.limit)
        except requests.RequestException as exc:
            print(f"Search failed for {query!r}: {exc}", file=sys.stderr)
            continue
        for page in pages:
            item = metadata(page)
            if not item:
                continue
            key = item["page_url"] or item["title"].casefold()
            if key in seen:
                continue
            seen.add(key)
            rows.append(item)
            if len(rows) >= args.limit:
                break
        print(f"{query}: {len(rows)}/{args.limit} unique image references", file=sys.stderr)
        if len(rows) >= args.limit:
            break

    rows.sort(key=lambda row: (row["theme"], row["title"].casefold()))
    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(args.output, "w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"Wrote {len(rows)} references to {args.output}")
    if len(rows) < args.limit:
        print(f"Commons returned {len(rows)} usable unique images (target: {args.limit}).", file=sys.stderr)


if __name__ == "__main__":
    main()
