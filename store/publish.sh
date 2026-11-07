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

# The store rejects any image that is not exactly the platform's screen size,
# and then publishes without screenshots, so check them first.
python3 - <<'EOF' || exit 1
import glob, os, sys
from PIL import Image
sizes = {"aplite": (144, 168), "basalt": (144, 168), "diorite": (144, 168),
         "flint": (144, 168), "chalk": (180, 180), "emery": (200, 228),
         "gabbro": (260, 260)}
bad = 0
for path in sorted(glob.glob("store/screenshots/*")):
    platform = os.path.basename(path).split("_", 1)[0]
    want, got = sizes.get(platform), Image.open(path).size
    if got != want:
        print("%s is %dx%d, the store needs %s" % (path, got[0], got[1],
              "%dx%d" % want if want else "a platform_ prefix"))
        bad = 1
sys.exit(bad)
EOF

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
