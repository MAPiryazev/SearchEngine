import time
import html
import subprocess
from urllib.parse import parse_qs, quote_plus
from http.server import BaseHTTPRequestHandler, HTTPServer

BOOL_SEARCH = "./build/bool_search"
INV_PATH = "out/index.inv"
FWD_PATH = "out/index.fwd"
PAGE_SIZE = 50

def run_search(q: str, top: int):
    cmd = [BOOL_SEARCH, "--inv", INV_PATH, "--fwd", FWD_PATH, "--q", q, "--top", str(top)]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        return None, p.stderr.strip()

    hits = None
    recs = []
    for line in p.stdout.splitlines():
        if line.startswith("hits="):
            try:
                hits = int(line.split("=", 1)[1])
            except:
                hits = None
            continue
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) >= 3:
            docid = parts[0]
            title = parts[1]
            url = "\t".join(parts[2:])
            recs.append((docid, title, url))
    return (hits, recs), None

def page_index(recs, page: int):
    start = page * PAGE_SIZE
    end = start + PAGE_SIZE
    return recs[start:end], start, end

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
            body = f"""<!doctype html><html><head><meta charset="utf-8"><title>Search</title></head>
<body>
<h1>Search</h1>
<form action="/search" method="get">
<input name="q" style="width:60%" value="{html.escape(q)}">
<button type="submit">Search</button>
</form>
</body></html>"""
            self._send_html(200, body, time.perf_counter() - t0)
            return

        if path == "/search":
            q = params.get("q", [""])[0]
            try:
                page = int(params.get("page", ["0"])[0])
            except:
                page = 0
            if page < 0:
                page = 0

            top = (page + 1) * PAGE_SIZE
            result, err = run_search(q, top)

            if err is not None:
                body = f"""<!doctype html><html><head><meta charset="utf-8"><title>Error</title></head>
<body>
<form action="/search" method="get">
<input name="q" style="width:60%" value="{html.escape(q)}">
<button type="submit">Search</button>
</form>
<pre>{html.escape(err)}</pre>
</body></html>"""
                self._send_html(500, body, time.perf_counter() - t0)
                return

            hits, recs = result
            chunk, _, end = page_index(recs, page)

            qenc = quote_plus(q)
            next_link = f"/search?q={qenc}&page={page+1}"
            prev_link = f"/search?q={qenc}&page={page-1}"

            items_html = []
            for docid, title, url in chunk:
                items_html.append(
                    f'<li><a href="{html.escape(url)}">{html.escape(title)}</a>'
                    f'<div style="color:#666;font-size:12px">{html.escape(url)}</div></li>'
                )

            pt = time.perf_counter() - t0

            body = f"""<!doctype html><html><head><meta charset="utf-8"><title>Results</title></head>
<body>
<form action="/search" method="get">
<input name="q" style="width:60%" value="{html.escape(q)}">
<button type="submit">Search</button>
</form>

<div>hits={hits if hits is not None else "?"}</div>
<div>time_ms={pt*1000.0:.3f}</div>

<ol>
{''.join(items_html)}
</ol>

<div>
{"<a href='" + prev_link + "'>Prev</a> | " if page > 0 else ""}
<a href="{next_link}">Next 50</a>
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
