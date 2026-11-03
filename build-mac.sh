#!/bin/bash
# Quick local build: compiles FL Chorus and puts VST3 + AU straight into
# ~/Library/Audio/Plug-Ins (no installer). Run in Terminal:  bash build-mac.sh
set -e
cd "$(dirname "$0")"

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Нужны Xcode Command Line Tools. Сейчас откроется установщик."
    xcode-select --install || true
    echo "Когда установка закончится, запусти этот скрипт ещё раз."
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "Ставлю CMake..."
    if command -v brew >/dev/null 2>&1; then
        brew install cmake
    else
        python3 -m pip install --user --quiet cmake
        export PATH="$(python3 -m site --user-base)/bin:$PATH"
    fi
fi

echo "Собираю (первый раз скачивается JUCE, это пара минут)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(sysctl -n hw.ncpu)"

VST3="$HOME/Library/Audio/Plug-Ins/VST3/FL Chorus.vst3"
AU="$HOME/Library/Audio/Plug-Ins/Components/FL Chorus.component"
for b in "$VST3" "$AU"; do
    [ -d "$b" ] && codesign --force --deep -s - "$b" >/dev/null 2>&1 || true
done
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "Готово:"
echo "  VST3 -> $VST3"
echo "  AU   -> $AU"
echo "В Ableton: Settings -> Plug-Ins -> Rescan."
