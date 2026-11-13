#!/bin/bash
# The release zips: the mod (shroudlink), the mod for EMF, the optional Greeneye Loader mod for map mod players,
# and the folder that runs shroudlink inside ShroudForge.
set -e
cd "$(dirname "$0")"
VERSION=$(sed -n 's/.*kVersion = "\(.*\)";/\1/p' src/expeditions.h)
./build_win.sh
compat/shroudlink-first/build_win.sh
OUT=build/release
rm -rf "$OUT" && mkdir -p "$OUT/mod/shroudlink_mods" "$OUT/compat/greeneye/0-shroudlink-first" "$OUT/emf/emf/mods/expeditions"
cp build/expeditions.dll "$OUT/mod/shroudlink_mods/"
cp dist/README-expeditions.txt dist/README-expeditions-ru.txt "$OUT/mod/"
cp compat/shroudlink-first/build/shroudlink-first.dll compat/shroudlink-first/mod.json "$OUT/compat/greeneye/0-shroudlink-first/"
cp dist/README-shroudlink-first.txt "$OUT/compat/greeneye/0-shroudlink-first/README.txt"
cp build/emf/expeditions.dll dist/emf/mod.json "$OUT/emf/emf/mods/expeditions/"
cp dist/README-expeditions-emf.txt dist/README-expeditions-emf-ru.txt "$OUT/emf/"
(cd "$OUT/mod" && zip -qr "../expeditions-$VERSION.zip" .)
(cd "$OUT/emf" && zip -qr "../expeditions-emf-$VERSION.zip" .)
COMPAT_VERSION=$(sed -n 's/.*"version": *"\(.*\)".*/\1/p' compat/shroudlink-first/mod.json)
(cd "$OUT/compat" && zip -qr "../expeditions-mapmod-compat-$COMPAT_VERSION.zip" .)
# the same Greeneye Loader mod as a Nexus mod of its own
(cd "$OUT/compat" && zip -qr "../shroudlink-first-$COMPAT_VERSION.zip" .)
# shroudlink inside ShroudForge: mods\shroudlink\native-plugin.ini and the guide (CRLF for Notepad)
SF_VERSION=$(sed -n 's/^shroudlink inside ShroudForge \(.*\)$/\1/p' dist/shroudforge/README-shroudlink-in-shroudforge.txt)
mkdir -p "$OUT/shroudforge/mods/shroudlink"
sed 's/$/\r/' dist/shroudforge/native-plugin.ini > "$OUT/shroudforge/mods/shroudlink/native-plugin.ini"
sed 's/$/\r/' dist/shroudforge/README-shroudlink-in-shroudforge.txt > "$OUT/shroudforge/README-shroudlink-in-shroudforge.txt"
(cd "$OUT/shroudforge" && zip -qr "../shroudlink-in-shroudforge-$SF_VERSION.zip" .)
ls -l "$OUT"/*.zip
