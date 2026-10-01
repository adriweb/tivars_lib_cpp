#!/usr/bin/env python3
"""Package CLI downloads, plus Quick Look on macOS, with SHA-256 checksums."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def package(platform, architecture, label, build_dir, output_dir):
    if not re.fullmatch(r'[A-Za-z0-9._-]+', label):
        raise ValueError('Package label must contain only letters, digits, dots, underscores or hyphens')
    name = f'tivars-{label}-{platform}-{architecture}'
    output_dir.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if platform == 'windows' else ''
    with tempfile.TemporaryDirectory(prefix='tivars-package-') as temporary:
        staging = Path(temporary) / name
        staging.mkdir()
        shutil.copy2(build_dir / ('tivars_lib_cpp_cli' + suffix), staging / ('tivars_cli' + suffix))
        for filename in ('LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.txt'):
            shutil.copy2(ROOT / filename, staging / filename)
        if platform == 'macos':
            # Preserve the stapled ticket, resource forks and bundle metadata.
            subprocess.run(['ditto', str(build_dir / 'TIVarsQuickLook.app'), str(staging / 'TIVarsQuickLook.app')], check=True)
            archive = output_dir / (name + '.zip')
            subprocess.run(['ditto', '-c', '-k', '--keepParent', str(staging), str(archive)], check=True)
        elif platform == 'windows':
            archive = output_dir / (name + '.zip')
            with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as stream:
                for path in sorted(staging.rglob('*')):
                    if path.is_file():
                        stream.write(path, path.relative_to(staging.parent))
        else:
            archive = output_dir / (name + '.tar.gz')
            with tarfile.open(archive, 'w:gz') as stream:
                stream.add(staging, arcname=name)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output_dir / 'SHA256SUMS').write_text(f'{checksum}  {archive.name}\n', encoding='utf-8')
    print(archive)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['linux', 'windows', 'macos'], required=True)
    parser.add_argument('--architecture', choices=['x86_64', 'universal'], required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/package')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/dist')
    args = parser.parse_args()
    package(args.platform, args.architecture, args.label, args.build_dir, args.output_dir)


if __name__ == '__main__':
    main()
