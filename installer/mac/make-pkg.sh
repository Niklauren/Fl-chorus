#!/bin/bash
# Builds FL Chorus (universal VST3 + AU) and packs it into one macOS installer:
#   dist/FL-Chorus-<version>-mac.pkg
# Needs Xcode Command Line Tools and CMake. Run from Terminal:  bash installer/mac/make-pkg.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$(pwd)"
VERSION="1.0.0"
NAME="FL Chorus"
ID="com.mikitalaurenau.flchorus"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"

command -v cmake >/dev/null 2>&1 || { echo "CMake not found: brew install cmake (or: python3 -m pip install --user cmake)"; exit 1; }

cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DFLC_COPY_AFTER_BUILD=OFF
cmake --build "$BUILD" --config Release -j"$(sysctl -n hw.ncpu)"
ART="$BUILD/FLChorus_artefacts/Release"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/pkgs" "$DIST"
cp -R "$ART/VST3/$NAME.vst3" "$STAGE/vst3/"
cp -R "$ART/AU/$NAME.component" "$STAGE/au/"

# Ad-hoc signature (no Apple developer account): required on Apple Silicon
codesign --force --deep -s - "$STAGE/vst3/$NAME.vst3"
codesign --force --deep -s - "$STAGE/au/$NAME.component"

# Bundles must install exactly into the plug-in folders, never be "relocated"
make_component() # <root> <plist>
{
    pkgbuild --analyze --root "$1" "$2" >/dev/null
    local n
    n=$(/usr/libexec/PlistBuddy -c "Print" "$2" | grep -c "BundleIsRelocatable" || true)
    for ((i = 0; i < n; i++)); do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$2"
    done
}
make_component "$STAGE/vst3" "$STAGE/vst3.plist"
make_component "$STAGE/au" "$STAGE/au.plist"

pkgbuild --root "$STAGE/vst3" --component-plist "$STAGE/vst3.plist" \
    --identifier "$ID.vst3" --version "$VERSION" \
    --install-location "/Library/Audio/Plug-Ins/VST3" "$STAGE/pkgs/vst3.pkg"

chmod +x "$ROOT/installer/mac/scripts/postinstall"
pkgbuild --root "$STAGE/au" --component-plist "$STAGE/au.plist" \
    --identifier "$ID.au" --version "$VERSION" \
    --scripts "$ROOT/installer/mac/scripts" \
    --install-location "/Library/Audio/Plug-Ins/Components" "$STAGE/pkgs/au.pkg"

OUT="$DIST/FL-Chorus-$VERSION-mac.pkg"
productbuild --distribution "$ROOT/installer/mac/distribution.xml" \
    --package-path "$STAGE/pkgs" \
    --resources "$ROOT/installer/mac/resources" \
    "$OUT"

echo ""
echo "Installer ready: $OUT"
