#!/usr/bin/env python3
"""Verify portable universal binaries, signatures, entitlements and notarization."""
import argparse
import os
from pathlib import Path
import plistlib
import re
import subprocess


def command(*arguments):
    result = subprocess.run(arguments, capture_output=True, check=True)
    return result.stdout + result.stderr


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(cli, app, release=False, notarized=False):
    release = release or notarized
    bundles = [app, app / 'Contents/PlugIns/TIVarsQuickLookPreview.appex',
               app / 'Contents/PlugIns/TIVarsQuickLookThumbnail.appex']
    executables = [cli]
    for bundle in bundles:
        info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
        executable = bundle / 'Contents/MacOS' / info['CFBundleExecutable']
        executables.append(executable)
    for executable in executables:
        require(executable.is_file(), f'Missing executable: {executable}')
        architectures = command('lipo', '-archs', str(executable)).decode().split()
        require(set(architectures) == {'arm64', 'x86_64'}, f'{executable}: expected arm64 and x86_64, got {architectures}')
        # No Homebrew/runner libraries or unresolved @rpath dependencies in downloads.
        for architecture in architectures:
            libraries = command('otool', '-arch', architecture, '-L', str(executable)).decode().splitlines()[1:]
            for line in libraries:
                dependency = line.strip().split(' (', 1)[0]
                require(dependency.startswith(('/usr/lib/', '/System/Library/')),
                        f'{executable}: nonportable dependency: {dependency}')
    teams = set()
    for item in [cli, *bundles]:
        command('codesign', '--verify', '--strict', str(item))
        if release:
            signature = command('codesign', '--display', '--verbose=4', str(item)).decode()
            require(re.search(r'^Authority=Developer ID Application: .+$', signature, re.M), f'{item}: missing Developer ID signature')
            require(re.search(r'^CodeDirectory .*\bruntime\b', signature, re.M), f'{item}: missing hardened runtime')
            require(re.search(r'^Timestamp=.+$', signature, re.M), f'{item}: missing secure timestamp')
            team = re.search(r'^TeamIdentifier=([A-Z0-9]{10})$', signature, re.M)
            require(team is not None, f'{item}: missing signing team')
            teams.add(team.group(1))
        if item in bundles:
            entitlements = plistlib.loads(subprocess.run(
                ['codesign', '--display', '--entitlements', ':-', str(item)], capture_output=True, check=True).stdout)
            require(entitlements.get('com.apple.security.app-sandbox') is True, f'{item}: missing sandbox')
            require(entitlements.get('com.apple.security.files.user-selected.read-only') is True, f'{item}: missing file access entitlement')
            require(entitlements.get('com.apple.security.get-task-allow', False) is False, f'{item}: debugger entitlement enabled')
    command('codesign', '--verify', '--deep', '--strict', str(app))
    if release:
        require(len(teams) == 1, 'CLI, app and extensions must share a signing team')
        expected_team = os.environ.get('APPLE_NOTARIZATION_TEAMID')
        if expected_team:
            require(teams == {expected_team}, 'Signature team differs from notarization team')
    if notarized:
        command('codesign', '--verify', '--strict', '--check-notarization', str(cli))
        command('xcrun', 'stapler', 'validate', str(app))
        assessment = command('spctl', '--assess', '--type', 'execute', '--verbose=2', str(app)).decode()
        require('source=Notarized Developer ID' in assessment, 'App did not pass Notarized Developer ID assessment')
    print(f'Verified {cli} and {app}' + (' (notarized)' if notarized else ''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cli', type=Path)
    parser.add_argument('app', type=Path)
    parser.add_argument('--release', action='store_true')
    parser.add_argument('--notarized', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.cli, args.app, args.release, args.notarized)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        if isinstance(error, subprocess.CalledProcessError):
            print((error.stdout + error.stderr).decode(errors='replace'))
        raise SystemExit(str(error)) from error


if __name__ == '__main__':
    main()
