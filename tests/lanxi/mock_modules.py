"""Loopback-only, original LAN-XI protocol fixture; never contacts hardware."""
import json
import math
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def packet(kind, payload, count=0):
    return struct.pack('<2sHHHI4BQ I', b'BK', 20, kind, 0, 0,
                       16, 0, 0, 0, count, len(payload)) + payload


def descriptor(signal, kind, data):
    return struct.pack('<4H', signal, kind, 0, len(data)) + data + b'\0' * (-len(data) % 4)


class Pair:
    def __init__(self, fault='none'):
        self.fault = fault
        self.log = []
        self.lock = threading.RLock()
        self.output_on = False
        self.level = 0.0
        self.frequency = 1024.0
        self.waveform = 'sine'
        self.failed_stop = False
        self.modules = [Module(self, 3676), Module(self, 3677)]

    def __enter__(self):
        for module in self.modules:
            module.start()
        return self

    def __exit__(self, *_):
        for module in self.modules:
            module.close()


class Module:
    def __init__(self, pair, model):
        self.pair = pair
        self.model = model
        self.state = 'Idle'
        self.running = threading.Event()
        self.closed = threading.Event()
        self.rate = 8192
        self.sequence = 0
        self.origin = 2**55 + model * 65536
        self.setup = self.defaults()
        self.stream = socket.socket()
        self.stream.bind(('127.0.0.1', 0))
        self.stream.listen(1)
        self.stream.settimeout(0.2)
        module = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def request(self):
                size = int(self.headers.get('Content-Length', '0'))
                if size > 131072:
                    self.send_error(413)
                    return
                raw = self.rfile.read(size)
                try:
                    body = json.loads(raw) if raw else None
                    status, response = module.handle(self.command, self.path, body)
                except (ValueError, KeyError, TypeError) as exc:
                    status, response = 400, {'error': str(exc)}
                data = json.dumps(response).encode()
                self.send_response(status)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            do_GET = request
            do_PUT = request
            do_POST = request

        self.http = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.url = f'http://127.0.0.1:{self.http.server_port}'

    def defaults(self):
        return {'channels': [dict(channel=c, enabled=True, name=f'Ch {c}',
                bandwidth='3.2 kHz', destinations=['socket'], range='10 Vpeak',
                filter='7.0 Hz', ccld=False, polVolt=False,
                transducer={'sensitivity': 1, 'unit': 'V', 'type': {'number': 'None'}})
                for c in range(1, 5)]}

    def info(self):
        return {'module': {'type': {'number': str(self.model)}, 'serial': self.model * 100,
                'version': {'firmware': '2.10.0.344'},
                'frontpanel': {'type': {'number': '3100' if self.model == 3677 else '2100', 'variant': '041'}}},
                'license': 'permanentLicense', 'moduleState': self.state,
                'numberOfInputChannels': 4, 'numberOfOutputChannels': int(self.model == 3677),
                'preampInputSupported': True, 'supportedSampleRates': [65536, 32768, 16384, 8192],
                'supportedRanges': ['10 Vpeak'], 'supportedFilters': ['7.0 Hz', 'DC']}

    def start(self):
        threading.Thread(target=self.http.serve_forever, daemon=True).start()
        threading.Thread(target=self.stream_loop, daemon=True).start()

    def close(self):
        self.closed.set()
        self.running.clear()
        self.http.shutdown()
        self.http.server_close()
        self.stream.close()

    def handle(self, method, path, body):
        with self.pair.lock:
            self.pair.log.append({'model': self.model, 'method': method, 'path': path,
                                  'body': body, 'state': self.state})
            path = path.removeprefix('/rest/rec')
            if method == 'GET':
                if path == '/module/info':
                    return 200, self.info()
                if path.startswith('/onchange'):
                    return 200, {'moduleState': self.state, 'lastUpdateTag': 1}
                if path == '/channels/input/default':
                    return 200, self.defaults()
                if path == '/destination/socket':
                    return 200, {'tcpPort': self.stream.getsockname()[1]}
                if path == '/channels/input/all/transducers':
                    return 200, [dict(unit='V', sensitivity=1.0, serialNumber=123,
                                      requiresCcld=0, requires200V=0), None, None, None]
            if method == 'POST' and path == '/channels/input/all/transducers/detect':
                return (200, {}) if self.state == 'Idle' else (409, {'error': 'Module busy'})
            if path == '/open' and self.state == 'Idle':
                if body.get('singleModule') is not True or body.get('performTransducerDetection') is not False:
                    return 400, {'error': 'Light modules must remain independent; no implicit TEDS'}
                self.state = 'RecorderOpened'
            elif path == '/create' and self.state == 'RecorderOpened':
                self.state = 'RecorderConfiguring'
            elif path == '/channels/input' and self.state == 'RecorderConfiguring':
                self.setup = body
                self.rate = round(float(body['channels'][0]['bandwidth'].split()[0]) * 2560)
                self.state = 'RecorderStreaming'
            elif path == '/measurements' and method == 'POST' and self.state == 'RecorderStreaming':
                if self.model == 3677 and self.pair.fault == 'start':
                    return 500, {'error': 'Injected second-module start failure'}
                self.state = 'RecorderRecording'
                self.sequence = 0
                self.running.set()
            elif path == '/measurements/stop' and self.state == 'RecorderRecording':
                self.running.clear()
                self.state = 'RecorderStreaming'
            elif path == '/finish' and self.state == 'RecorderStreaming':
                self.state = 'RecorderOpened'
            elif path == '/cancel' and self.state == 'RecorderConfiguring':
                self.state = 'RecorderOpened'
            elif path == '/close' and self.state == 'RecorderOpened':
                self.state = 'Idle'
            elif path.startswith('/generator/') and self.model == 3677:
                if path.endswith('/stop'):
                    if self.pair.fault == 'stop' and self.pair.output_on and not self.pair.failed_stop:
                        self.pair.failed_stop = True
                        return 500, {'error': 'Injected output-stop failure'}
                    self.pair.output_on = False
                elif path.endswith('/prepare'):
                    self.pair.output_on = False
                    self.pair.level = 0
                elif path.endswith('/output'):
                    output = body['outputs'][0]
                    self.pair.level = output['gain'] * 10
                    source = output['inputs'][0]
                    self.pair.frequency = source.get('frequency', source.get('center_frequency', 1024))
                    self.pair.waveform = source['signalType']
                elif path.endswith('/start'):
                    if not all(m.state == 'RecorderRecording' for m in self.pair.modules):
                        return 400, {'error': 'Generator started before both input streams'}
                    self.pair.output_on = True
                else:
                    return 404, {'error': 'Unknown generator command'}
            else:
                return 403, {'error': f'Invalid command {method} {path} in {self.state}'}
            return 200, {}

    def stream_loop(self):
        while not self.closed.is_set():
            try:
                connection, _ = self.stream.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            with connection:
                sent_descriptors = False
                try:
                    while not self.closed.is_set():
                        if not self.running.wait(0.05):
                            continue
                        enabled = [ch for ch in self.setup['channels'] if ch['enabled']]
                        if not sent_descriptors:
                            descriptions = b''
                            for signal, ch in enumerate(enabled, 1):
                                unit = ch['transducer']['unit'].encode()
                                descriptions += descriptor(signal, 1, struct.pack('<H', 3))
                                descriptions += descriptor(signal, 2, struct.pack('<d', 10 / ch['transducer']['sensitivity']))
                                descriptions += descriptor(signal, 4, struct.pack('<4BQ', 16, 0, 0, 0, 65536 // self.rate))
                                descriptions += descriptor(signal, 5, struct.pack('<H', len(unit)) + unit)
                            wire = packet(8, descriptions)
                            connection.sendall(wire[:7])  # Deliberately split a header.
                            connection.sendall(wire[7:])
                            sent_descriptors = True
                        count = 256
                        payload = struct.pack('<HH', len(enabled), 0)
                        with self.pair.lock:
                            frequency = self.pair.frequency if self.pair.output_on else 1024
                            level = self.pair.level if self.pair.output_on else 0.1
                        for signal, ch in enumerate(enabled, 1):
                            physical = ch['channel']
                            gain = [1, 2, 0.5, 1][physical - 1]
                            phase = [0, 0.3, -0.2, 0][physical - 1]
                            payload += struct.pack('<HH', signal, count)
                            samples = bytearray()
                            for n in range(count):
                                t = (self.sequence + n) / self.rate
                                value = level * gain * math.cos(2 * math.pi * frequency * t + phase)
                                code = max(-8388608, min(8388607, round(value / 10 * 8388608)))
                                samples.extend((code & 0xffffff).to_bytes(3, 'little'))
                            payload += samples
                        wire = packet(1, payload, self.origin + self.sequence * (65536 // self.rate))
                        if self.pair.fault == 'truncated' and self.model == 3676 and self.sequence >= self.rate:
                            connection.sendall(wire[:35])
                            break
                        connection.sendall(wire)
                        self.sequence += count
                        if self.pair.fault == 'disconnect' and self.model == 3676 and self.sequence >= self.rate:
                            break
                        time.sleep(count / self.rate)
                except (ConnectionError, OSError):
                    pass
