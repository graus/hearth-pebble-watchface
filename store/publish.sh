#!/bin/sh
# Publishes Hearth to the Pebble app store (appstore-api.repebble.com).
#
# Run from anywhere, after `pebble login` (a browser sign-in) on this machine:
#     store/publish.sh
#
# Note: pebble-tool 5.1.2 sends every upload as published and visible (its
# --is-published flag is ignored), so this goes LIVE straight away. Test on a
# real watch first. The script asks for confirmation before uploading.
#
# Source URL: the public repository, unless HEARTH_SOURCE_URL overrides it.
# (Passed explicitly so it never depends on how this clone's remote is set.)
set -eu
cd "$(dirname "$0")/.."

for f in store/icon-80.png store/icon-144.png store/description.txt store/release-notes.txt; do
  [ -f "$f" ] || { echo "missing $f (run: python3 tools/build_store_assets.py)"; exit 1; }
done
ls store/screenshots/*.gif store/screenshots/*.png >/dev/null 2>&1 || {
  echo "no screenshots in store/screenshots"; exit 1; }

# `pebble login --status` succeeds even when logged out, so read its output.
status=$(pebble login --status 2>&1) || true
echo "$status"
case "$status" in
  *"logged out"*) echo "Not signed in to the store: run 'pebble login' first."; exit 1 ;;
esac

echo
echo "About to publish Hearth $(python3 -c "import json;print(json.load(open('package.json'))['version'])") to the Pebble app store."
echo "This makes it public immediately. Type 'publish' to continue:"
read answer
[ "$answer" = "publish" ] || { echo "Aborted."; exit 1; }

pebble build

set -- --non-interactive \
  --pbw build/*.pbw \
  --description "$(cat store/description.txt)" \
  --release-notes "$(cat store/release-notes.txt)" \
  --icon-small store/icon-80.png \
  --icon-large store/icon-144.png \
  --screenshots store/screenshots/*.gif store/screenshots/*.png

pebble publish "$@" --source "${HEARTH_SOURCE_URL:-https://github.com/graus/hearth-pebble-watchface}"
