#!/usr/bin/env python3
"""Read public cast API metadata without credentials or state changes."""
import argparse
import datetime
import json
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urlsplit
from urllib.request import urlopen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", default="http://127.0.0.1:8766")
    parser.add_argument("--date", help="Published date in YYYY-MM-DD format")
    args = parser.parse_args()
    base = args.base_url.rstrip("/")
    origin = urlsplit(base)
    if (origin.scheme not in ("http", "https") or not origin.hostname
            or origin.path or origin.query or origin.fragment
            or origin.username or origin.password):
        parser.error("--base-url must be an HTTP(S) origin without credentials or a path")
    if args.date:
        try:
            day = datetime.date.fromisoformat(args.date)
            if day.isoformat() != args.date or not 2020 <= day.year <= 2099:
                raise ValueError()
        except ValueError:
            parser.error("--date must be YYYY-MM-DD between 2020 and 2099")

    def get(path):
        with urlopen(base + path, timeout=10) as response:
            return json.load(response)

    try:
        summary = {
            "origin": base,
            "health": get("/healthz"),
            "cities": [city["city"] for city in get("/api/config")["cities"]],
            "actor_count": len(get("/api/actors")["actors"]),
            "published_dates": get("/api/published-dates")["dates"],
        }
        if args.date:
            summary["lineup"] = get("/api/lineup?" + urlencode({"date": args.date}))
    except HTTPError as error:
        print("HTTP " + str(error.code) + ": check the date and API guide", file=sys.stderr)
        return 1
    except (URLError, TimeoutError, OSError, ValueError, KeyError) as error:
        print("Cannot read API: " + type(error).__name__ + "; check the server and base URL", file=sys.stderr)
        return 1
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
