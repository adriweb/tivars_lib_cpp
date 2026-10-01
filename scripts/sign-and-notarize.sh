#!/bin/bash
# Reuses the Developer ID secret names from Safari WebUSB / CEmu.
set -euo pipefail
set +x
umask 077
stage=initialization
trap 'status=$?; printf "Signing failed during: %s (exit %s)\n" "$stage" "$status" >&2; exit "$status"' ERR
progress() { stage="$1"; printf '%s\n' "$stage"; }
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_root"
cli="${1:-build/package/tivars_lib_cpp_cli}"
app="${2:-build/package/TIVarsQuickLook.app}"
required=(MACOS_CERTIFICATE MACOS_CERTIFICATE_PWD MACOS_KEYCHAIN_PWD MACOS_CODESIGN_IDENT
  APPLE_NOTARIZATION_USERNAME APPLE_NOTARIZATION_PASSWORD APPLE_NOTARIZATION_TEAMID)
missing=()
for name in "${required[@]}"; do
  if [[ -z "${!name:-}" ]]; then missing+=("$name"); fi
done
if (( ${#missing[@]} )); then
  printf 'Missing signing/notarization secret: %s\n' "${missing[@]}" >&2
  exit 1
fi
test -f "$cli"
test -d "$app/Contents/PlugIns/TIVarsQuickLookPreview.appex"
test -d "$app/Contents/PlugIns/TIVarsQuickLookThumbnail.appex"
temporary=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/tivars-sign.XXXXXX")
keychain="$temporary/signing.keychain-db"
restore_search_list=false
cleanup() {
  if [[ "$restore_search_list" == true ]]; then
    python3 - "$temporary/keychains.txt" <<'PY' || true
import pathlib, shlex, subprocess, sys
original = shlex.split(pathlib.Path(sys.argv[1]).read_text())
subprocess.run(['security', 'list-keychains', '-d', 'user', '-s', *original], check=True)
PY
  fi
  security delete-keychain "$keychain" >/dev/null 2>&1 || true
  rm -rf "$temporary"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
python3 - "$temporary/certificate.p12" <<'PY'
import base64, os, pathlib, sys
certificate = ''.join(os.environ['MACOS_CERTIFICATE'].split())
pathlib.Path(sys.argv[1]).write_bytes(base64.b64decode(certificate, validate=True))
PY
progress 'Creating temporary signing keychain'
security create-keychain -p "$MACOS_KEYCHAIN_PWD" "$keychain"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$MACOS_KEYCHAIN_PWD" "$keychain"
# Private-key lookup needs the imported keychain in the user search list too.
security list-keychains -d user > "$temporary/keychains.txt"
restore_search_list=true
python3 - "$temporary/keychains.txt" "$keychain" <<'PY'
import pathlib, shlex, subprocess, sys
original = shlex.split(pathlib.Path(sys.argv[1]).read_text())
subprocess.run(['security', 'list-keychains', '-d', 'user', '-s', sys.argv[2], *original], check=True)
PY
progress 'Importing and authorizing signing key'
security import "$temporary/certificate.p12" -k "$keychain" -P "$MACOS_CERTIFICATE_PWD" \
  -T /usr/bin/codesign -T /usr/bin/security >/dev/null
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$MACOS_KEYCHAIN_PWD" "$keychain" >/dev/null
security find-identity -v -p codesigning "$keychain"
signing=(--force --sign "$MACOS_CODESIGN_IDENT" --keychain "$keychain" --timestamp --options runtime --generate-entitlement-der)
progress 'Signing CLI and embedded Quick Look extensions'
codesign "${signing[@]}" --identifier com.adriweb.tivars-lib-cpp.cli "$cli"
codesign "${signing[@]}" --entitlements quicklook/Preview.entitlements "$app/Contents/PlugIns/TIVarsQuickLookPreview.appex"
codesign "${signing[@]}" --entitlements quicklook/Thumbnail.entitlements "$app/Contents/PlugIns/TIVarsQuickLookThumbnail.appex"
progress 'Signing and verifying containing app'
codesign "${signing[@]}" --entitlements quicklook/App.entitlements "$app"
python3 scripts/verify-macos-distribution.py "$cli" "$app" --release
progress 'Validating notarization credentials'
xcrun notarytool store-credentials tivars --keychain "$keychain" \
  --apple-id "$APPLE_NOTARIZATION_USERNAME" --password "$APPLE_NOTARIZATION_PASSWORD" \
  --team-id "$APPLE_NOTARIZATION_TEAMID" >/dev/null
progress 'Submitting CLI and app together for notarization'
mkdir "$temporary/submission"
ditto "$cli" "$temporary/submission/tivars_cli"
ditto "$app" "$temporary/submission/TIVarsQuickLook.app"
ditto -c -k --keepParent "$temporary/submission" "$temporary/notarization.zip"
if ! xcrun notarytool submit "$temporary/notarization.zip" --keychain-profile tivars \
  --keychain "$keychain" --wait --timeout 20m --output-format json > "$temporary/result.json"; then
  echo 'Notarization submission did not complete.' >&2
  cat "$temporary/result.json" >&2
  exit 1
fi
status=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["status"])' "$temporary/result.json")
if [[ "$status" != Accepted ]]; then
  submission=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$temporary/result.json")
  xcrun notarytool log "$submission" --keychain-profile tivars --keychain "$keychain" "$temporary/notarization-log.json"
  cat "$temporary/notarization-log.json" >&2
  exit 1
fi
progress 'Stapling app ticket and verifying notarization'
# Standalone Mach-O executables cannot be stapled; the CLI ticket is checked online.
xcrun stapler staple "$app"
python3 scripts/verify-macos-distribution.py "$cli" "$app" --notarized
echo 'CLI and Quick Look Developer ID signing and notarization succeeded.'
