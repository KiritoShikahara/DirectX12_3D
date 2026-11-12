#!/usr/bin/env bash
# 戦闘ルールの単体テストを cl.exe でビルドして実行する
cd "$(dirname "$0")/.."
VSDIR="$("/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath)"
mkdir -p x64/Tests
cat > x64/Tests/build.cmd <<CMD
@echo off
call "$VSDIR\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /EHsc /utf-8 /W3 /I Tests\stub /I Source Tests\CombatRulesTests.cpp Source\Application\Combat\CombatRules.cpp /Fox64\Tests\ /Fex64\Tests\CombatRulesTests.exe
CMD
if ! cmd //c "x64\Tests\build.cmd" > x64/Tests/build.log 2>&1; then
  cat x64/Tests/build.log; echo "FAIL unit test build"; exit 1
fi
x64/Tests/CombatRulesTests.exe
