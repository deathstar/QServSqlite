# A simple python webserver that displays stats and ranks players from your servers saved statistics database
# To utilize the script, install python, run: nohup python stats-webserver.py & and then forward port 8080 using TCP
# Anyone can see your webpage with saved statistics from http://yourIP:8080. You can use tinyurl.com to hide the IP

import sqlite3, os, threading, time, re, html, urllib.parse, shlex
from http.server import SimpleHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

PORT = 8080
DB_FILE = 'playerinfo.db'
FLAGRUN_FILE = os.environ.get('FLAGRUN_FILE', 'config/flagruns.cfg')
CACHE_REFRESH_INTERVAL = 60

# Cache for all 5 modes: weighted (default), kd, frags, deaths, flags
html_cache = {m: b"<h3>Initializing...</h3>" for m in ['weighted', 'kd', 'frags', 'deaths', 'flags']}
flagruns_cache = b"<h3>Initializing flagruns...</h3>"
cache_lock = threading.Lock()

MODE_LABELS = {
    12: "Insta CTF",
    17: "Effic CTF",
    11: "CTF",
}

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """Handle requests in separate threads to prevent webserver hangs."""
    daemon_threads = True

def clean_name(name_bytes):
    name_no_colors = re.sub(b'[\x0c\x0f].', b'', name_bytes)
    clean_bytes = bytes([b for b in name_no_colors if 32 <= b <= 126])
    return clean_bytes.decode('ascii', errors='ignore').strip() or "UnknownPlayer"

def format_flagrun_ms(ms):
    if ms < 60_000:
        return f"{ms / 1000:.2f}s"
    minutes = ms // 60_000
    seconds = (ms % 60_000) / 1000
    return f"{minutes}:{seconds:05.2f}"

def get_flagruns_by_map(filepath=FLAGRUN_FILE):
    """Return sorted list of best flagruns per map: (map_name, time_str, player_escaped, mode_label)."""
    if not os.path.exists(filepath):
        return []

    best_per_map = {}
    with open(filepath, 'rb') as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith(b'//') or line.startswith(b'#'):
                continue

            line_str = line.decode('latin-1', errors='replace')
            try:
                tokens = shlex.split(line_str, posix=True)
            except ValueError:
                continue

            if len(tokens) < 5 or tokens[0] != 'flagrun':
                continue

            try:
                mode_num = int(tokens[1])
                map_name = tokens[2].strip()
                time_ms = int(tokens[3])
                raw_player = tokens[4].encode('latin-1', errors='replace')
            except ValueError:
                continue

            if not map_name or time_ms < 1500:
                continue

            player_clean = html.escape(clean_name(raw_player))
            mode_label = MODE_LABELS.get(mode_num, f"Mode {mode_num}")

            existing = best_per_map.get(map_name)
            if existing is None or time_ms < existing['time_ms']:
                best_per_map[map_name] = {
                    'map': map_name,
                    'time_ms': time_ms,
                    'time_str': format_flagrun_ms(time_ms),
                    'player': player_clean,
                    'mode': mode_label,
                }

    sorted_maps = sorted(best_per_map.values(), key=lambda item: item['map'].lower())
    return [
        (html.escape(item['map']), item['time_str'], item['player'], html.escape(item['mode']))
        for item in sorted_maps
    ]

def get_leaderboard_data(sort_mode):
    sort_map = {
        'kd': 'CAST(KD AS REAL) DESC',
        'frags': 'CAST(FRAGS AS REAL) DESC',
        'deaths': 'CAST(DEATHS AS REAL) DESC',
        'flags': 'CAST(FLAGS AS REAL) DESC',
        'weighted': '(CAST(KD AS REAL) * 100) + (CAST(FLAGS AS REAL) * 25) + (CAST(FRAGS AS REAL) * 0.1) DESC'
    }
    order_by = sort_map.get(sort_mode, sort_map['weighted'])

    conn = sqlite3.connect(f'file:{DB_FILE}?mode=ro', uri=True, timeout=5.0)
    conn.text_factory = bytes
    cursor = conn.cursor()
    try:
        cursor.execute(f"SELECT NAME, FRAGS, DEATHS, FLAGS, ROUND(CAST(KD AS REAL), 2) FROM PLAYERINFO ORDER BY {order_by} LIMIT 100")
        raw_rows = cursor.fetchall()
        return [(html.escape(clean_name(r[0])), r[1], r[2], r[3], r[4]) for r in raw_rows]
    finally:
        conn.close()

def shared_styles():
    return """
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;600&display=swap');
    body { background: #050505; color: #fff; font-family: 'Inter', sans-serif; display: flex; justify-content: center; padding: 40px 20px; margin: 0; }
    .wrapper { width: 100%; max-width: 900px; background: #0a0a0c; border: 1px solid #1a1a1a; padding: 30px; border-radius: 20px; }
    .topbar { display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 16px; margin-bottom: 30px; }
    h2 { font-weight: 300; color: #3b82f6; text-transform: uppercase; letter-spacing: 4px; margin: 0; }
    nav { display: flex; gap: 20px; }
    nav a { text-decoration: none; color: #6b7280; text-transform: uppercase; font-size: 12px; letter-spacing: 2px; padding-bottom: 6px; border-bottom: 1px solid transparent; }
    nav a:hover { color: #e5e7eb; }
    nav a.active { color: #3b82f6; border-bottom: 1px solid #3b82f6; }
    table { width: 100%; border-collapse: collapse; }
    th { padding: 15px; text-align: left; border-bottom: 1px solid #1a1a1a; color: #4b5563; text-transform: uppercase; font-size: 11px; letter-spacing: 2px; }
    th a { text-decoration: none; color: #4b5563; text-transform: uppercase; font-size: 11px; letter-spacing: 2px; padding-bottom: 5px; }
    td { padding: 18px 15px; font-size: 14px; border-bottom: 1px solid #111; }
    tr:hover { background: #0f0f12; }
    .rank, .record-time { font-weight: 600; color: #3b82f6; }
    .empty { color: #6b7280; text-align: center; padding: 30px 15px; }
    """

def build_html_layout(rows, active_mode):
    def active(m): return "color: #3b82f6; border-bottom: 1px solid #3b82f6;" if active_mode == m else ""

    return f"""<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Leaderboard</title>
<link rel="icon" type="image/png" sizes="32x32" href="https://stats.fm/icons/favicon-32x32.png"/>
<style>{shared_styles()}</style></head><body><div class="wrapper">
<div class="topbar">
  <h2><a href="/" style="text-decoration:none; color:inherit;">Leaderboard</a></h2>
  <nav>
    <a href="/" class="active">Leaderboard</a>
    <a href="/flagruns">Flagruns</a>
  </nav>
</div>
<table><tr><th>Rank</th><th>Player</th>
<th><a href="/?sort=frags" style="{active('frags')}">Frags</a></th>
<th><a href="/?sort=deaths" style="{active('deaths')}">Deaths</a></th>
<th><a href="/?sort=flags" style="{active('flags')}">Flags</a></th>
<th><a href="/?sort=kd" style="{active('kd')}">K/D Ratio</a></th></tr>
{"".join(f"<tr><td class='rank'>#{i}</td><td>{r[0]}</td><td>{r[1]}</td><td>{r[2]}</td><td>{r[3]}</td><td>{r[4]}</td></tr>" for i, r in enumerate(rows, 1))}
</table></div></body></html>"""

def build_flagruns_layout(rows):
    body_rows = "".join(
        f"<tr><td class='rank'>{r[0]}</td><td class='record-time'>{r[1]}</td><td>{r[2]}</td><td>{r[3]}</td></tr>"
        for r in rows
    ) or "<tr><td class='empty' colspan='4'>No flagruns recorded yet.</td></tr>"

    return f"""<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Flagruns by Map</title>
<link rel="icon" type="image/png" sizes="32x32" href="https://stats.fm/icons/favicon-32x32.png"/>
<style>{shared_styles()}</style></head><body><div class="wrapper">
<div class="topbar">
  <h2><a href="/flagruns" style="text-decoration:none; color:inherit;">Flagruns</a></h2>
  <nav>
    <a href="/">Leaderboard</a>
    <a href="/flagruns" class="active">Flagruns</a>
  </nav>
</div>
<table><tr><th>Map</th><th>Best Time</th><th>Holder</th><th>Mode</th></tr>
{body_rows}
</table></div></body></html>"""

def refresh_all_caches():
    global html_cache, flagruns_cache
    if os.path.exists(DB_FILE):
        for mode in list(html_cache.keys()):
            try:
                rows = get_leaderboard_data(mode)
                rendered = build_html_layout(rows, mode).encode('utf-8')
                with cache_lock:
                    html_cache[mode] = rendered
            except Exception as e:
                print(f"Update error ({mode}): {e}")

    try:
        flagrun_rows = get_flagruns_by_map(FLAGRUN_FILE)
        rendered_flagruns = build_flagruns_layout(flagrun_rows).encode('utf-8')
        with cache_lock:
            flagruns_cache = rendered_flagruns
    except Exception as e:
        print(f"Flagruns update error: {e}")

def update_cache():
    while True:
        time.sleep(CACHE_REFRESH_INTERVAL)
        refresh_all_caches()

class FastDBHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)

        if parsed.path == '/flagruns':
            with cache_lock:
                payload = flagruns_cache
            self.send_response(200)
            self.send_header('Content-type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        sort_mode = params.get('sort', ['weighted'])[0]
        with cache_lock:
            payload = html_cache.get(sort_mode, html_cache['weighted'])

        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

if __name__ == '__main__':
    refresh_all_caches()
    threading.Thread(target=update_cache, daemon=True).start()

    server = ThreadedHTTPServer(('0.0.0.0', PORT), FastDBHandler)
    print(f"Server started on port {PORT}...")
    server.serve_forever()
