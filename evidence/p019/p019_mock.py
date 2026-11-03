# P0-19 隔离测试用 Ollama Mock（支持 normal / delay8 / error / badjson 四种模式）
import json, time, http.server

MODE_FILE = '/tmp/p019_mode'
LOG = '/tmp/p019_mock.log'
COUNT = {'n': 0}


def read_mode():
    try:
        with open(MODE_FILE) as f:
            return f.read().strip()
    except Exception:
        return 'normal'


class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        COUNT['n'] += 1
        mode = read_mode()
        try:
            last = json.loads(body)['messages'][-1]['content']
        except Exception:
            last = '<unparsed>'
        with open(LOG, 'a') as f:
            f.write('REQ %d mode=%s text=%s\n' % (COUNT['n'], mode, last))

        if mode == 'error':
            self.send_response(500)
            self.send_header('Content-Length', '0')
            self.end_headers()
            return

        if mode == 'badjson':
            payload = b'this is not json'
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        if mode == 'delay8':
            time.sleep(8)

        resp = json.dumps({
            'model': 'mock',
            'message': {'role': 'assistant', 'content': 'MOCK_REPLY_%d' % COUNT['n']},
            'done': True,
        }).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(resp)))
        self.end_headers()
        self.wfile.write(resp)

    def log_message(self, *args):
        pass


print('MOCK_READY', flush=True)
http.server.HTTPServer(('127.0.0.1', 11438), H).serve_forever()
