import time
import html
import re
import subprocess
from urllib.parse import parse_qs, quote_plus
from http.server import BaseHTTPRequestHandler, HTTPServer

# Binaries
BOOL_SEARCH = "./build/bool_search"
RANK_SEARCH = "./build/rank_search8"

# Index paths
BOOL_INV_PATH = "out/index.inv"
BOOL_FWD_PATH = "out/index.fwd"
RANK_INV_PATH = "out6/index6.inv"
RANK_FWD_PATH = "out6/index6.fwd"

PAGE_SIZE = 50

_MD_LINK_RE = re.compile(r'^\[(https?://[^\]]+)\]\((https?://[^)]+)\)$')

def _normalize_url(u: str) -> str:
    u = (u or "").strip()
    m = _MD_LINK_RE.match(u)
    if m:
        return m.group(2).strip()
    return u

def run_search(mode: str, q: str, top: int):
    mode = (mode or "bool").strip().lower()

    if mode == "rank":
        cmd = [RANK_SEARCH, "--inv", RANK_INV_PATH, "--fwd", RANK_FWD_PATH, "--q", q, "--top", str(top)]
    else:
        cmd = [BOOL_SEARCH, "--inv", BOOL_INV_PATH, "--fwd", BOOL_FWD_PATH, "--q", q, "--top", str(top)]

    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        return None, p.stderr.strip()

    hits = None
    recs = []

    for raw in p.stdout.splitlines():
        line = raw.rstrip("\n")
        if not line:
            continue

        if line.startswith("hits="):
            try:
                hits = int(line.split("=", 1)[1])
            except:
                hits = None
            continue

        if line.startswith("hits "):
            try:
                hits = int(line.split(None, 1)[1])
            except:
                hits = None
            continue

        if mode == "rank":
            # Вариант 1 (как в эталоне): "docid score title\turl"
            if "\t" in line:
                left, url = line.split("\t", 1)
                url = _normalize_url(url)

                left_parts = left.split(" ", 2)
                if len(left_parts) < 3:
                    continue
                docid, score, title = left_parts[0], left_parts[1], left_parts[2]
                recs.append((docid, title, url, score))
                continue

            # Вариант 2 (как у тебя в терминале): "docid score title url" (url — последний токен)
            toks = line.split()
            if len(toks) < 4:
                continue
            docid = toks[0]
            score = toks[1]
            url = _normalize_url(toks[-1])
            title = " ".join(toks[2:-1])
            recs.append((docid, title, url, score))
            continue

        # bool mode
        if "\t" in line:
            parts = line.split("\t")
            if len(parts) >= 3:
                docid = parts[0]
                title = parts[1]
                url = _normalize_url("\t".join(parts[2:]))
                recs.append((docid, title, url, None))
            continue

        # fallback: "docid title url" (url — последний токен)
        toks = line.split()
        if len(toks) >= 3:
            docid = toks[0]
            url = _normalize_url(toks[-1])
            title = " ".join(toks[1:-1])
            recs.append((docid, title, url, None))

    return (hits, recs), None


def page_index(recs, page: int):
    start = page * PAGE_SIZE
    end = start + PAGE_SIZE
    return recs[start:end], start, end


def mode_select_html(mode: str):
    bool_sel = "selected" if mode != "rank" else ""
    rank_sel = "selected" if mode == "rank" else ""
    return (
        "<select name=\"mode\">"
        f"<option value=\"bool\" {bool_sel}>Boolean</option>"
        f"<option value=\"rank\" {rank_sel}>TF-IDF</option>"
        "</select>"
    )


class Handler(BaseHTTPRequestHandler):
    def end_headers(self):
        pt = getattr(self, "_process_time", None)
        if pt is not None:
            self.send_header("X-Process-Time", f"{pt:.6f}")
            self.send_header("Server-Timing", f"app;dur={pt*1000.0:.3f}")
        super().end_headers()

    def _send_html(self, status: int, body: str, process_time: float):
        self._process_time = process_time
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(body.encode("utf-8"))

    def do_GET(self):
        t0 = time.perf_counter()

        path, _, qs = self.path.partition("?")
        params = parse_qs(qs)

        if path == "/favicon.ico":
            self._process_time = time.perf_counter() - t0
            self.send_response(204)
            self.end_headers()
            return

        if path == "/":
            q = params.get("q", [""])[0]
            mode = params.get("mode", ["bool"])[0]
            body = f"""<!doctype html><html><head><meta charset=\"utf-8\"><title>Search</title></head>
<body>
<h1>Search</h1>
<form action=\"/search\" method=\"get\">
{mode_select_html(mode)}
<input name=\"q\" style=\"width:60%\" value=\"{html.escape(q)}\">
<button type=\"submit\">Search</button>
</form>
</body></html>"""
            self._send_html(200, body, time.perf_counter() - t0)
            return

        if path == "/search":
            q = params.get("q", [""])[0]
            mode = params.get("mode", ["bool"])[0]

            try:
                page = int(params.get("page", ["0"])[0])
            except:
                page = 0
            if page < 0:
                page = 0

            top = (page + 1) * PAGE_SIZE
            result, err = run_search(mode, q, top)

            if err is not None:
                body = f"""<!doctype html><html><head><meta charset=\"utf-8\"><title>Error</title></head>
<body>
<form action=\"/search\" method=\"get\">
{mode_select_html(mode)}
<input name=\"q\" style=\"width:60%\" value=\"{html.escape(q)}\">
<button type=\"submit\">Search</button>
</form>
<pre>{html.escape(err)}</pre>
</body></html>"""
                self._send_html(500, body, time.perf_counter() - t0)
                return

            hits, recs = result
            chunk, _, _ = page_index(recs, page)

            qenc = quote_plus(q)
            menc = quote_plus(mode)

            next_link = f"/search?q={qenc}&mode={menc}&page={page+1}"
            prev_link = f"/search?q={qenc}&mode={menc}&page={page-1}"

            items_html = []
            for docid, title, url, score in chunk:
                score_html = f"<div style=\"color:#444;font-size:12px\">score={html.escape(str(score))}</div>" if score is not None else ""
                items_html.append(
                    f"<li><a href=\"{html.escape(url)}\">{html.escape(title)}</a>"
                    f"{score_html}"
                    f"<div style=\"color:#666;font-size:12px\">{html.escape(url)}</div></li>"
                )

            pt = time.perf_counter() - t0
            mode_label = "TF-IDF" if mode == "rank" else "Boolean"

            body = f"""<!doctype html><html><head><meta charset=\"utf-8\"><title>Results</title></head>
<body>
<form action=\"/search\" method=\"get\">
{mode_select_html(mode)}
<input name=\"q\" style=\"width:60%\" value=\"{html.escape(q)}\">
<button type=\"submit\">Search</button>
</form>

<div>mode={html.escape(mode_label)}</div>
<div>hits={hits if hits is not None else "?"}</div>
<div>time_ms={pt*1000.0:.3f}</div>

<ol>
{''.join(items_html)}
</ol>

<div>
{"<a href='" + prev_link + "'>Prev</a> | " if page > 0 else ""}
<a href=\"{next_link}\">Next 50</a>
</div>

</body></html>"""
            self._send_html(200, body, pt)
            return

        self._send_html(
            404,
            "<!doctype html><html><head><meta charset='utf-8'><title>404</title></head><body>not found</body></html>",
            time.perf_counter() - t0,
        )


def main():
    host = "0.0.0.0"
    port = 8080
    httpd = HTTPServer((host, port), Handler)
    print(f"Listening on http://127.0.0.1:{port}/")
    httpd.serve_forever()


if __name__ == "__main__":
    main()