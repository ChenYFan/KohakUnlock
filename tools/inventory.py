"""Collect selected PCI device metadata; no MMIO, reset, unbind or firmware operations."""
import argparse
import datetime
import json
import re
import subprocess
from pathlib import Path


def collect(bdfs, sysfs=Path('/sys/bus/pci/devices')):
    result = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'devices': {}}
    for bdf in bdfs:
        if not re.fullmatch(r'[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]', bdf):
            raise ValueError(f'invalid full PCI address: {bdf}')
        base = sysfs / bdf.lower()
        if not base.exists():
            raise ValueError(f'device not found: {bdf}')
        row = {}
        for name in ('vendor', 'device', 'subsystem_vendor', 'subsystem_device', 'revision', 'resource', 'boot_vga', 'numa_node', 'reset_method'):
            try:
                row[name] = (base / name).read_text().strip()
            except OSError:
                row[name] = None
        row['driver'] = (base / 'driver').resolve().name if (base / 'driver').exists() else None
        row['iommu_group'] = (base / 'iommu_group').resolve().name if (base / 'iommu_group').exists() else None
        row['iommu_members'] = sorted(p.name for p in (base / 'iommu_group/devices').glob('*'))
        result['devices'][bdf] = row
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bdf', action='append', required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    if args.output.exists():
        p.error('output exists; choose a new private file')
    try:
        result = collect(args.bdf)
        for command in (['uname', '-r'], ['nvidia-smi', '--query-gpu=pci.bus_id,name,vbios_version,memory.total,driver_version', '--format=csv']):
            try:
                r = subprocess.run(command, capture_output=True, text=True, timeout=15)
                result[command[0]] = {'returncode': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr}
            except (OSError, subprocess.SubprocessError) as error:
                result[command[0]] = {'error': str(error)}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('x') as f:
            json.dump(result, f, indent=2)
    except (ValueError, OSError) as error:
        p.error(str(error))
    print(f'Saved {args.output}; inspect/redact before sharing. No GPU state changed.')


if __name__ == '__main__':
    main()
