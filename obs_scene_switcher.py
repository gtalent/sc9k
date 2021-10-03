from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import threading
import obspython as obs


def set_current_scene(scene_name):
    scenes = obs.obs_frontend_get_scenes()
    for scene in scenes:
        name = obs.obs_source_get_name(scene)
        if name == scene_name:
            obs.obs_frontend_set_current_scene(scene)
            return 0
    return 1


class RqstHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        up = urlparse(self.path)
        if up.path == '/Scene':
            qc = parse_qs(up.query)
            set_current_scene(qc.get('name', [''])[0])
            self.send_response(200)
            self.end_headers()
        elif up.path == '/ping':
            self.send_response(200)
            self.end_headers()


    def log_message(self, format, *args):
        pass


def run(name):
    httpd = HTTPServer(('127.0.0.1', 9302), RqstHandler)
    httpd.serve_forever()


t = threading.Thread(target=run, args=(1,), daemon=True)
t.start()
