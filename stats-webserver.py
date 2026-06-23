# A simple python webserver that displays stats and ranks players from your servers saved statistics database
# To utilize the script, install python, run: nohup python stats-webserver.py & and then forward port 8080 using TCP
# Anyone can see your webpage with saved statistics from http://yourIP:8080. You can use tinyurl.com to hide the IP

import sqlite3
import os
import threading
import time
from http.server import SimpleHTTPRequestHandler, HTTPServer

PORT = 8080 
DB_FILE = 'playerinfo.db'
CACHE_REFRESH_INTERVAL = 60 # Refresh stats every 60 seconds

# Keep the cache strictly as raw, ready-to-ship network bytes from the start
cached_html_bytes = b"<h3>Leaderboard is initializing... Please refresh in a moment.</h3>"
cache_lock = threading.Lock()

def update_leaderboard_cache():
    """Background worker that continuously updates the cached HTML."""
    global cached_html_bytes
    while True:
        if os.path.exists(DB_FILE):
            try:
                # Open in read-only mode 
                conn = sqlite3.connect(f'file:{DB_FILE}?mode=ro', uri=True)
                conn.text_factory = bytes
                cursor = conn.cursor()
                
                # Fetching data
                cursor.execute("SELECT NAME, FRAGS, DEATHS, FLAGS, ROUND(KD, 2) FROM PLAYERINFO ORDER BY FRAGS DESC LIMIT 100")
                raw_rows = cursor.fetchall()
                conn.close()
                
                # Process bytes safely
                rows = []
                for row in raw_rows:
                    try:
                        name = row[0].decode('utf-8', errors='replace')
                    except Exception:
                        name = row[0].decode('latin1', errors='replace')
                    rows.append((name, row[1], row[2], row[3], row[4]))
                
                # Generate the static HTML block
                new_html_str = build_html_layout(rows)
                
                # Thread-safe swap of the global cache, baked natively into raw bytes
                with cache_lock:
                    cached_html_bytes = new_html_str.encode('utf-8')
                    
            except Exception as e:
                error_msg = f"<h3>Cache update error: {e}</h3>"
                with cache_lock:
                    cached_html_bytes = error_msg.encode('utf-8')
        else:
            error_msg = f"<h3>Error: {DB_FILE} not found. Ensure this script is running in your QServ directory.</h3>"
            with cache_lock:
                cached_html_bytes = error_msg.encode('utf-8')
                
        time.sleep(CACHE_REFRESH_INTERVAL)

def build_html_layout(rows):
    """Generates the HTML string using the provided data rows."""
    html = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Leaderboard</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <style>
            body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #121212; color: #e0e0e0; padding: 20px; margin: 0; }
            .container { max-width: 900px; margin: 0 auto; }
            h2 { color: #4CAF50; border-bottom: 2px solid #2e7d32; padding-bottom: 10px; margin-bottom: 20px; }
            .table-responsive { overflow-x: auto; box-shadow: 0 4px 6px rgba(0,0,0,0.3); border-radius: 8px; }
            table { border-collapse: collapse; width: 100%; background: #1e1e1e; }
            th, td { padding: 14px 16px; text-align: left; border-bottom: 1px solid #2c2c2c; }
            th { background-color: #2e7d32; color: white; text-transform: uppercase; font-size: 13px; letter-spacing: 0.5px; }
            tr:hover { background-color: #2a2a2a; }
            .rank { font-weight: bold; color: #888; width: 50px; }
            .footer { margin-top: 20px; font-size: 12px; color: #666; text-align: center; }
        </style>
    </head>
    <body>
        <div class="container">
            <h2>Live Leaderboard</h2>
            <div class="table-responsive">
                <table>
                    <tr>
                        <th class="rank">Rank</th>
                        <th>Player Name</th>
                        <th>Frags</th>
                        <th>Deaths</th>
                        <th>Flags</th>
                        <th>K/D Ratio</th>
                    </tr>
    """
    for rank, row in enumerate(rows, start=1):
        html += f"""
        <tr>
            <td class="rank">#{rank}</td>
            <td>{row[0]}</td>
            <td>{row[1]}</td>
            <td>{row[2]}</td>
            <td>{row[3]}</td>
            <td>{row[4]}</td>
        </tr>
        """
        
    html += f"</table></div><div class='footer'>Leaderboard caches globally. Next refresh in {CACHE_REFRESH_INTERVAL}s.</div></div></body></html>"
    return html

class FastDBHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/' or self.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-type', 'text/html; charset=utf-8')
            self.send_header('Cache-Control', 'public, max-age=30')
            self.end_headers()
            
            # Instantly slice the memory stream to the client without translating formats
            with cache_lock:
                response_data = cached_html_bytes
                
            self.wfile.write(response_data)
        else:
            self.send_error(404, "File Not Found")

if __name__ == '__main__':
    updater_thread = threading.Thread(target=update_leaderboard_cache, daemon=True)
    updater_thread.start()
    
    server = HTTPServer(('0.0.0.0', PORT), FastDBHandler)
    print(f"🌐 QServ Stats Webserver is up and listening on port {PORT}...")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping stats webserver.")
        server.server_close()
