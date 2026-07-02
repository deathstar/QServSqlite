import sqlite3, os, threading, time, re, html, urllib.parse
from http.server import SimpleHTTPRequestHandler, HTTPServer

PORT = 8080
DB_FILE = 'playerinfo.db'
CACHE_REFRESH_INTERVAL = 60

# Cache for all 5 modes: weighted (default), kd, frags, deaths, flags
html_cache = {m: b"<h3>Initializing...</h3>" for m in ['weighted', 'kd', 'frags', 'deaths', 'flags']}
cache_lock = threading.Lock()

def clean_name(name_bytes):
    name_no_colors = re.sub(b'[\x0c\x0f].', b'', name_bytes)
    clean_bytes = bytes([b for b in name_no_colors if 32 <= b <= 126])
    return clean_bytes.decode('ascii', errors='ignore').strip() or "UnknownPlayer"

def get_leaderboard_data(sort_mode):
    sort_map = {
        'kd': 'CAST(KD AS REAL) DESC',
        'frags': 'CAST(FRAGS AS REAL) DESC',
        'deaths': 'CAST(DEATHS AS REAL) DESC',
        'flags': 'CAST(FLAGS AS REAL) DESC',
        'weighted': '(CAST(KD AS REAL) * 100) + (CAST(FLAGS AS REAL) * 25) + (CAST(FRAGS AS REAL) * 0.1) DESC'
    }
    order_by = sort_map.get(sort_mode, sort_map['weighted'])
    
    conn = sqlite3.connect(f'file:{DB_FILE}?mode=ro', uri=True)
    conn.text_factory = bytes
    cursor = conn.cursor()
    cursor.execute(f"SELECT NAME, FRAGS, DEATHS, FLAGS, ROUND(CAST(KD AS REAL), 2) FROM PLAYERINFO ORDER BY {order_by} LIMIT 100")
    raw_rows = cursor.fetchall()
    conn.close()
    return [(html.escape(clean_name(r[0])), r[1], r[2], r[3], r[4]) for r in raw_rows]

def update_cache():
    global html_cache
    while True:
        if os.path.exists(DB_FILE):
            for mode in html_cache.keys():
                try:
                    rows = get_leaderboard_data(mode)
                    with cache_lock:
                        html_cache[mode] = build_html_layout(rows, mode).encode('utf-8')
                except Exception as e:
                    print(f"Update error ({mode}): {e}")
        time.sleep(CACHE_REFRESH_INTERVAL)

def build_html_layout(rows, active_mode):
    def active(m): return "color: #3b82f6; border-bottom: 1px solid #3b82f6;" if active_mode == m else ""
    
    return f"""<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;600&display=swap');
    body {{ background: #050505; color: #fff; font-family: 'Inter', sans-serif; display: flex; justify-content: center; padding: 40px 20px; }}
    .wrapper {{ width: 100%; max-width: 900px; background: #0a0a0c; border: 1px solid #1a1a1a; padding: 30px; border-radius: 20px; }}
    h2 {{ font-weight: 300; color: #3b82f6; text-transform: uppercase; letter-spacing: 4px; margin-bottom: 30px; }}
    table {{ width: 100%; border-collapse: collapse; }}
    th {{ padding: 15px; text-align: left; border-bottom: 1px solid #1a1a1a; }}
    th a {{ text-decoration: none; color: #4b5563; text-transform: uppercase; font-size: 11px; letter-spacing: 2px; padding-bottom: 5px; }}
    td {{ padding: 18px 15px; font-size: 14px; border-bottom: 1px solid #111; }}
    tr:hover {{ background: #0f0f12; }}
    .rank {{ font-weight: 600; color: #3b82f6; }}
</style></head><body><div class="wrapper">
<h2><a href="/" style="text-decoration:none; color:inherit;">Leaderboard</a></h2>
<table><tr><th>Rank</th><th>Player</th>
<th><a href="/?sort=frags" style="{active('frags')}">Frags</a></th>
<th><a href="/?sort=deaths" style="{active('deaths')}">Deaths</a></th>
<th><a href="/?sort=flags" style="{active('flags')}">Flags</a></th>
<th><a href="/?sort=kd" style="{active('kd')}">K/D Ratio</a></th></tr>
{"".join(f"<tr><td class='rank'>#{i}</td><td>{r[0]}</td><td>{r[1]}</td><td>{r[2]}</td><td>{r[3]}</td><td>{r[4]}</td></tr>" for i, r in enumerate(rows, 1))}
</table></div></body></html>"""

class FastDBHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)
        sort_mode = params.get('sort', ['weighted'])[0]
        
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        with cache_lock:
            self.wfile.write(html_cache.get(sort_mode, html_cache['weighted']))

if __name__ == '__main__':
    threading.Thread(target=update_cache, daemon=True).start()
    HTTPServer(('0.0.0.0', PORT), FastDBHandler).serve_forever()
