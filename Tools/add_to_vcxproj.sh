#!/usr/bin/env bash
# 使い方: Tools/add_to_vcxproj.sh <path/to/file.cpp|hpp|hlsl> ...
# vcxproj / filters に未登録のソースを登録する
set -e
cd "$(dirname "$0")/.."
proj="DirectX12 Engine.vcxproj"
for f in "$@"; do
  win="${f//\//\\}"
  case "$f" in
    *.cpp) tag=ClCompile ;;
    *.hpp|*.h) tag=ClInclude ;;
    *) tag=None ;;
  esac
  if grep -qF "Include=\"$win\"" "$proj"; then continue; fi
  anchor=$(grep -n "<$tag Include=" "$proj" | head -1 | cut -d: -f1)
  if [ -z "$anchor" ]; then anchor=$(grep -n "<ClInclude Include=" "$proj" | head -1 | cut -d: -f1); fi
  sed -i "${anchor}i\\    <$tag Include=\"${win//\\/\\\\}\" />\r" "$proj"
  echo "added $f"
done
