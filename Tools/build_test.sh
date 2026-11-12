#!/usr/bin/env bash
# ビルドテスト + スモークテスト
#   1. Debug / Release (x64) をビルド
#   2. 各シーンを固定フレーム数だけ自動実行し、異常終了・エラーログが無いことを確認
#   3. 最終フレームを Document/test/<config>_<scene>.png に保存（目視確認用）
# 使い方: bash Tools/build_test.sh [Debug|Release|all] [frames]
set -u
cd "$(dirname "$0")/.."

CONFIGS="${1:-all}"
FRAMES="${2:-240}"
[ "$CONFIGS" = "all" ] && CONFIGS="Debug Release"

VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
MSBUILD="$("$VSWHERE" -latest -find 'MSBuild\**\Bin\MSBuild.exe' | head -1)"
if [ -z "$MSBUILD" ]; then echo "FAIL: MSBuild not found"; exit 1; fi

# シーン番号:名前  (0:Title 1:StageSelect 2:InGame 3:GameClear 4:GameOver 5:Tutorial)
SCENES="0:title 1:stageselect 2:ingame 5:tutorial 3:gameclear 4:gameover"
EXTRA_RUNS="2:ingame_autoplay:--autoplay --enemy 2"

mkdir -p Document/test
fail=0

run_case() {
  local cfg="$1" scene="$2" name="$3" extra="$4"
  local exe="x64/$cfg/DirectX12 Engine.exe"
  local png="Document/test/${cfg}_${name}.png"
  local before; before=$(wc -l < Log/engine.log 2>/dev/null || echo 0)
  rm -f "$png"
  # shellcheck disable=SC2086
  timeout 180 "$exe" --frames "$FRAMES" --scene "$scene" --capture "$(cygpath -w "$PWD/$png")" $extra
  local code=$?
  local errs; errs=$(tail -n +"$((before + 1))" Log/engine.log 2>/dev/null | grep -E "\[(error|critical)\]" | head -3)
  if [ $code -ne 0 ]; then
    echo "FAIL [$cfg] $name: exit code $code"; fail=1
  elif [ -n "$errs" ]; then
    echo "FAIL [$cfg] $name: error log"; echo "$errs"; fail=1
  elif [ ! -s "$png" ]; then
    echo "FAIL [$cfg] $name: screenshot missing"; fail=1
  else
    echo "PASS [$cfg] $name ($png)"
  fi
}

for cfg in $CONFIGS; do
  echo "== Build $cfg"
  if ! "$MSBUILD" "DirectX12 Engine.sln" -p:Configuration="$cfg" -p:Platform=x64 -m -v:m -nologo > "Document/test/build_$cfg.log" 2>&1; then
    echo "FAIL [$cfg] build"; grep -E " error " "Document/test/build_$cfg.log" | head -10; fail=1; continue
  fi
  echo "PASS [$cfg] build"
  for s in $SCENES; do run_case "$cfg" "${s%%:*}" "${s##*:}" ""; done
  IFS=':' read -r sc nm ex <<< "$EXTRA_RUNS"; run_case "$cfg" "$sc" "$nm" "$ex"
done

if [ $fail -ne 0 ]; then echo "RESULT: FAIL"; exit 1; fi
echo "RESULT: PASS"
