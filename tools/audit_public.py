"""Check tracked files for common accidental publication of machine-private data."""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BAD_SUFFIXES = {'.bin', '.rom', '.efi', '.ko', '.o', '.zip', '.gz', '.xz', '.log', '.out'}
PATTERNS = {
    'GPU UUID': r'GPU-[0-9a-fA-F]{8}-[0-9a-fA-F-]{27,}',
    'filesystem UUID': r'root=UUID=[0-9a-fA-F-]{36}',
    'private IPv4': r'\b(?:192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})\b',
    'private key': r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----',
    'GitHub token': r'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})',
    'home directory': r'(?:/home/[A-Za-z0-9_.-]+/|C:[\\/]Users[\\/][A-Za-z0-9_.-]+[\\/])',
}


def main():
    files = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    errors = []
    for name in filter(None, files):
        path = ROOT / name
        if path.suffix.lower() in BAD_SUFFIXES or name.split('/')[0] in {'assets', 'local', 'private', 'build'}:
            errors.append(f'{name}: forbidden artifact/path')
        raw = path.read_bytes()
        if b'\0' in raw:
            errors.append(f'{name}: binary content')
            continue
        text = raw.decode('utf-8')
        for label, pattern in PATTERNS.items():
            if re.search(pattern, text):
                errors.append(f'{name}: possible {label}')
    for error in errors:
        print(error)
    if errors:
        return 1
    print('Tracked-file publication checks passed. Heuristic checks do not replace review.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
