#!/usr/bin/env bash
# 開発用: Debugビルド + 1シーン実行してスクショ
# 使い方: bash Tools/quick.sh <scene> <frames> [extra args...]
cd "$(dirname "$0")/.."
cfg="${CFG:-Debug}"
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" "DirectX12 Engine.sln" -p:Configuration=$cfg -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -E " error |warning C4[0-9]+.*(PostProcess|SceneRenderer)|->" | head -20
scene="${1:-2}"; frames="${2:-120}"; shift 2 2>/dev/null
before=$(wc -l < Log/engine.log 2>/dev/null || echo 0)
rm -f Document/quick.png
timeout 150 "x64/$cfg/DirectX12 Engine.exe" --frames "$frames" --scene "$scene" --capture "$(cygpath -w "$PWD")\\Document\\quick.png" "$@"
echo "exit=$?"
tail -n +"$((before + 1))" Log/engine.log | grep -vE "Loaded audio" | tail -8
ls -la Document/quick.png 2>&1 | awk '{print $5, $NF}'
