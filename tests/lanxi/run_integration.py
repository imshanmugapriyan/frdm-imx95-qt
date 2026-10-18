"""Run the compiled Qt backend against isolated loopback fixture modules."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from mock_modules import Pair


def main():
    executable = str(Path(sys.argv[1]).resolve())
    for scenario in ('none', 'steps', 'sequence', 'start', 'stop', 'disconnect', 'truncated'):
        with tempfile.TemporaryDirectory(prefix='suno-lanxi-') as temporary, Pair(scenario) as pair:
            env = dict(os.environ, SUNO_LANXI_DATA_DIR=temporary)
            result = subprocess.run([executable, *(m.url for m in pair.modules), scenario],
                                    env=env, capture_output=True, text=True, timeout=50)
            print(result.stdout, end='', flush=True)
            if result.returncode:
                print(result.stderr)
                print(json.dumps(pair.log[-20:], indent=2))
                raise SystemExit(result.returncode)
            assert all(m.state == 'Idle' for m in pair.modules), 'Peer was not rolled back'
            assert not pair.output_on, 'Generator remained on'
            if scenario in ('none', 'steps', 'sequence', 'truncated'):
                csv_files = [p for p in Path(temporary).rglob('*.csv') if 'exports' in p.parts]
                wav_files = list(Path(temporary).rglob('*.wav'))
                assert len(csv_files) == len(wav_files) == 8
                for wav in wav_files:
                    data = wav.read_bytes()
                    assert data[:4] == b'RIFF' and int.from_bytes(data[4:8], 'little') + 8 == len(data)
                    assert int.from_bytes(data[44:48], 'little') * 4 == int.from_bytes(data[52:56], 'little')
    print('Seven Qt backend integration scenarios passed.')


if __name__ == '__main__':
    main()
