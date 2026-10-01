"""Exercise signing failure paths with dummy Apple tools, never real credentials."""
import base64
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SECRETS = {
    'MACOS_CERTIFICATE': base64.b64encode(b'dummy-p12').decode(),
    'MACOS_CERTIFICATE_PWD': 'dummy-cert-password',
    'MACOS_KEYCHAIN_PWD': 'dummy-keychain-password',
    'MACOS_CODESIGN_IDENT': 'Developer ID Application: Dummy (ABCDEFGHIJ)',
    'APPLE_NOTARIZATION_USERNAME': 'dummy@example.invalid',
    'APPLE_NOTARIZATION_PASSWORD': 'dummy-notary-password',
    'APPLE_NOTARIZATION_TEAMID': 'ABCDEFGHIJ',
}
FAKE_TOOL = r'''#!/usr/bin/env python3
import json, os, pathlib, shutil, sys
name, args = pathlib.Path(sys.argv[0]).name, sys.argv[1:]
with open(os.environ['FAKE_LOG'], 'a') as stream:
    stream.write(json.dumps({'command': name, 'args': args}) + '\n')
if os.environ.get('FAKE_FAIL') == name + ':' + args[0]:
    sys.exit(42)
if name == 'security' and args == ['list-keychains', '-d', 'user']:
    print('"/dummy/login.keychain-db"\n"/dummy/System.keychain"')
elif name == 'security' and args[0] == 'import':
    assert pathlib.Path(args[1]).read_bytes() == b'dummy-p12'
elif name == 'ditto':
    if args[0] == '-c':
        source = pathlib.Path(args[-2])
        if source.name == 'submission':
            assert (source / 'tivars_cli').is_file()
            assert (source / 'TIVarsQuickLook.app').is_dir()
        pathlib.Path(args[-1]).write_bytes(b'dummy-zip')
    elif pathlib.Path(args[0]).is_dir():
        shutil.copytree(args[0], args[1])
    else:
        shutil.copyfile(args[0], args[1])
elif name == 'xcrun' and args[:2] == ['notarytool', 'submit']:
    print(json.dumps({'id': 'dummy-submission', 'status': os.environ.get('FAKE_STATUS', 'Accepted')}))
    sys.exit(int(os.environ.get('FAKE_SUBMIT_EXIT', '0')))
elif name == 'xcrun' and args[:2] == ['notarytool', 'log']:
    pathlib.Path(args[-1]).write_text('{"issues": ["Dummy rejection"]}')
'''
FAKE_VERIFIER = r'''import json, os, sys
with open(os.environ['FAKE_LOG'], 'a') as stream:
    stream.write(json.dumps({'command': 'verify', 'args': sys.argv[1:]}) + '\n')
if os.environ.get('FAKE_VERIFY_FAIL') in sys.argv:
    sys.exit(42)
'''


class SigningTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='tivars-sign-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        scripts = self.root / 'scripts'
        scripts.mkdir()
        shutil.copyfile(ROOT / 'scripts/sign-and-notarize.sh', scripts / 'sign-and-notarize.sh')
        (scripts / 'verify-macos-distribution.py').write_text(FAKE_VERIFIER)
        self.cli = self.root / 'build/package/tivars_lib_cpp_cli'
        self.cli.parent.mkdir(parents=True)
        self.cli.write_bytes(b'dummy-cli')
        self.app = self.cli.parent / 'TIVarsQuickLook.app'
        for name in ('Preview', 'Thumbnail'):
            (self.app / f'Contents/PlugIns/TIVarsQuickLook{name}.appex').mkdir(parents=True)
        fake_bin = self.root / 'bin'
        fake_bin.mkdir()
        for name in ('security', 'codesign', 'xcrun', 'ditto'):
            path = fake_bin / name
            path.write_text(FAKE_TOOL)
            path.chmod(0o755)
        self.runner_temp = self.root / 'runner-temp'
        self.runner_temp.mkdir()
        self.log = self.root / 'commands.jsonl'
        self.env = {key: value for key, value in os.environ.items() if key not in SECRETS and not key.startswith('FAKE_')}
        self.env.update(SECRETS, PATH=str(fake_bin) + os.pathsep + os.environ['PATH'],
                        RUNNER_TEMP=str(self.runner_temp), FAKE_LOG=str(self.log))

    def run_script(self, overrides=None, missing=None):
        environment = self.env.copy()
        environment.update(overrides or {})
        if missing:
            environment.pop(missing)
        result = subprocess.run(['bash', str(self.root / 'scripts/sign-and-notarize.sh')],
                                env=environment, capture_output=True, text=True)
        self.records = [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []
        return result

    def assert_cleaned_up(self):
        self.assertEqual(list(self.runner_temp.iterdir()), [])
        self.assertEqual(self.records[-2], {'command': 'security', 'args': [
            'list-keychains', '-d', 'user', '-s', '/dummy/login.keychain-db', '/dummy/System.keychain']})
        self.assertEqual(self.records[-1]['args'][0], 'delete-keychain')

    def test_success_signs_inside_out_and_submits_both_binaries(self):
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        signing = [record for record in self.records if record['command'] == 'codesign']
        self.assertEqual([Path(record['args'][-1]).name for record in signing], [
            'tivars_lib_cpp_cli', 'TIVarsQuickLookPreview.appex', 'TIVarsQuickLookThumbnail.appex', 'TIVarsQuickLook.app'])
        for record in signing:
            self.assertIn('--timestamp', record['args'])
            self.assertIn('runtime', record['args'])
        stapling = [record for record in self.records if record['args'][:2] == ['stapler', 'staple']]
        self.assertEqual(len(stapling), 1)
        self.assertEqual(stapling[0]['args'][-1], str(self.app.relative_to(self.root)))
        verified = [record['args'][-1] for record in self.records if record['command'] == 'verify']
        self.assertEqual(verified, ['--release', '--notarized'])
        self.assert_cleaned_up()

    def test_missing_secrets_never_touches_keychain(self):
        result = self.run_script(missing='MACOS_CERTIFICATE')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('MACOS_CERTIFICATE', result.stderr)
        self.assertEqual(self.records, [])

    def test_failed_certificate_import_restores_search_list(self):
        result = self.run_script({'FAKE_FAIL': 'security:import'})
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(record['command'] == 'codesign' for record in self.records))
        self.assert_cleaned_up()

    def test_signed_verification_failure_never_submits(self):
        result = self.run_script({'FAKE_VERIFY_FAIL': '--release'})
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(record['command'] == 'xcrun' for record in self.records))
        self.assert_cleaned_up()

    def test_failed_submission_never_staples(self):
        result = self.run_script({'FAKE_SUBMIT_EXIT': '42'})
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(record['args'][0] == 'stapler' for record in self.records))
        self.assert_cleaned_up()

    def test_rejection_fetches_log_without_stapling(self):
        result = self.run_script({'FAKE_STATUS': 'Invalid'})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Dummy rejection', result.stderr)
        self.assertTrue(any(record['args'][:2] == ['notarytool', 'log'] for record in self.records))
        self.assertFalse(any(record['args'][0] == 'stapler' for record in self.records))
        self.assert_cleaned_up()

    def test_stapler_failure_does_not_report_success(self):
        result = self.run_script({'FAKE_FAIL': 'xcrun:stapler'})
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('succeeded', result.stdout)
        self.assert_cleaned_up()

    def test_notarized_verification_failure_does_not_report_success(self):
        result = self.run_script({'FAKE_VERIFY_FAIL': '--notarized'})
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('succeeded', result.stdout)
        self.assert_cleaned_up()


class PackageTests(unittest.TestCase):
    def test_linux_and_windows_contents_permissions_and_checksums(self):
        spec = importlib.util.spec_from_file_location('package_binaries', ROOT / 'scripts/package-binaries.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for platform, extension in [('linux', ''), ('windows', '.exe')]:
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as temporary:
                build = Path(temporary) / 'build'
                build.mkdir()
                binary = build / ('tivars_lib_cpp_cli' + extension)
                binary.write_bytes(b'dummy-cli')
                binary.chmod(0o755)
                output = Path(temporary) / 'dist'
                archive = module.package(platform, 'x86_64', 'test', build, output)
                prefix = f'tivars-test-{platform}-x86_64/'
                if platform == 'linux':
                    with tarfile.open(archive) as stream:
                        member = stream.getmember(prefix + 'tivars_cli')
                        self.assertEqual(member.mode & 0o111, 0o111)
                        self.assertEqual(stream.extractfile(member).read(), b'dummy-cli')
                        names = stream.getnames()
                else:
                    with zipfile.ZipFile(archive) as stream:
                        self.assertEqual(stream.read(prefix + 'tivars_cli.exe'), b'dummy-cli')
                        names = stream.namelist()
                self.assertIn(prefix + 'LICENSE', names)
                self.assertIn(prefix + 'README.md', names)
                self.assertIn(prefix + 'THIRD_PARTY_NOTICES.txt', names)
                self.assertEqual((output / 'SHA256SUMS').read_text(),
                                 f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')


class VerificationTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('verify_macos', ROOT / 'scripts/verify-macos-distribution.py')
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.cli = self.root / 'tivars_cli'
        self.cli.write_bytes(b'dummy-cli')
        self.app = self.root / 'TIVarsQuickLook.app'
        for bundle, name in [(self.app, 'TIVarsQuickLook'),
                             (self.app / 'Contents/PlugIns/TIVarsQuickLookPreview.appex', 'TIVarsQuickLookPreview'),
                             (self.app / 'Contents/PlugIns/TIVarsQuickLookThumbnail.appex', 'TIVarsQuickLookThumbnail')]:
            contents = bundle / 'Contents'
            (contents / 'MacOS').mkdir(parents=True)
            (contents / 'Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': name}))
            (contents / 'MacOS' / name).write_bytes(b'dummy-executable')
        self.architectures = b'x86_64 arm64\n'
        self.dependency = '/usr/lib/libSystem.B.dylib'
        self.signature = ('Authority=Developer ID Application: Dummy\n'
                          'CodeDirectory v=20500 flags=0x10000(runtime)\n'
                          'Timestamp=Oct 1, 2026\nTeamIdentifier=ABCDEFGHIJ\n')
        self.entitlements = {'com.apple.security.app-sandbox': True,
                             'com.apple.security.files.user-selected.read-only': True}
        self.assessment = b'accepted\nsource=Notarized Developer ID\n'
        self.calls = []
        patch = mock.patch.object(self.module.subprocess, 'run', side_effect=self.fake_run)
        patch.start()
        self.addCleanup(patch.stop)

    def fake_run(self, arguments, **kwargs):
        arguments = list(arguments)
        self.calls.append(arguments)
        output = b''
        if arguments[0] == 'lipo':
            output = self.architectures
        elif arguments[0] == 'otool':
            output = f'dummy:\n\t{self.dependency} (compatibility version 1.0.0, current version 1.0.0)\n'.encode()
        elif arguments[:2] == ['codesign', '--display']:
            output = plistlib.dumps(self.entitlements) if '--entitlements' in arguments else self.signature.encode()
        elif arguments[0] == 'spctl':
            output = self.assessment
        return subprocess.CompletedProcess(arguments, 0, stdout=output, stderr=b'')

    def verify(self, **kwargs):
        # Ignore any real local Apple credential environment for these dummy signatures.
        with mock.patch.dict(os.environ, {'APPLE_NOTARIZATION_TEAMID': ''}):
            self.module.verify(self.cli, self.app, **kwargs)

    def test_notarization_checks_cli_online_and_validates_stapled_app(self):
        self.verify(notarized=True)
        self.assertIn(['codesign', '--verify', '--strict', '--check-notarization', str(self.cli)], self.calls)
        self.assertIn(['xcrun', 'stapler', 'validate', str(self.app)], self.calls)

    def test_rejects_missing_architecture(self):
        self.architectures = b'arm64\n'
        with self.assertRaisesRegex(ValueError, 'expected arm64 and x86_64'):
            self.verify()

    def test_rejects_homebrew_dependency(self):
        self.dependency = '/opt/homebrew/lib/libc++.1.dylib'
        with self.assertRaisesRegex(ValueError, 'nonportable dependency'):
            self.verify()

    def test_rejects_adhoc_release_signature(self):
        self.signature = 'Signature=adhoc\n'
        with self.assertRaisesRegex(ValueError, 'missing Developer ID'):
            self.verify(release=True)

    def test_rejects_release_without_timestamp(self):
        self.signature = self.signature.replace('Timestamp=Oct 1, 2026\n', '')
        with self.assertRaisesRegex(ValueError, 'missing secure timestamp'):
            self.verify(release=True)

    def test_rejects_release_without_hardened_runtime(self):
        self.signature = self.signature.replace('(runtime)', '(none)')
        with self.assertRaisesRegex(ValueError, 'missing hardened runtime'):
            self.verify(release=True)

    def test_rejects_debugger_entitlement(self):
        self.entitlements['com.apple.security.get-task-allow'] = True
        with self.assertRaisesRegex(ValueError, 'debugger entitlement'):
            self.verify(release=True)

    def test_rejects_gatekeeper_assessment_without_notarization(self):
        self.assessment = b'accepted\nsource=Developer ID\n'
        with self.assertRaisesRegex(ValueError, 'Notarized Developer ID assessment'):
            self.verify(notarized=True)


if __name__ == '__main__':
    unittest.main()
