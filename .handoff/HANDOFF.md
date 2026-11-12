# 引き継ぎ (2026-11-12 21:50 / remaster)

次の一手: `bash Tools/build_test.sh Release` を実行して PASS を確認し、Release を起動してタイトル → ステージ選択 → 戦闘の UI を目で確認する。

## 完了
- Debug 起動時のクラッシュを修正（Logger の初期化順と DXGI デバッグファクトリ）
- ソースを UTF-8 に統一し、`/utf-8` を指定
- HDR 描画（シャドウ、PBR と IBL、SSAO、Bloom、フォグ、ACES、FXAA）と月面 PBR 地形（21c6816、`FrameWork/Graphics/PostProcess/SceneRenderer.cpp`）
- 戦闘：パリィ・ガード・体幹・スタミナ・ヒットストップ・ロックオン、自作パーティクル、チュートリアル、単体テストとスモークテスト（714fcfc、08d6b6a）
- 操作表記をキーマウス／パッドで自動で切り替え、アニメーション補間で細くなる問題を修正（44fc329）
- UI の作り直し（UiTheme、HUD、タイトルの文字を消した画像、各画面の操作表記）（068aa52、`Source/Application/UI/`）
- 設計書 `Document/GameDesign.md`、使い方 `README.md`

## 残り（優先順・最大5件）
- ゲームオーバー（Title / Retry ボタンの画像）とクリア画面の見た目を、新しい UI テーマに合わせる
- フェード中もタイトルの操作表記が見えてしまう問題（`TitleScene.cpp` の Render でフェードの α を掛ける）
- 次の話題「画面構成とステージ構成」（ステージが全部同じ月面の円盤なので、変化を付ける）
- 実機で手触りを調整する（`Source/Application/Combat/CombatRules.hpp` の数値）
- Effekseer をリンクから外す（ゲーム中ではもう使っていない）

## 保留
- 実機での確認（パッドでの表記切り替え、走りながら旋回したときの細り）— ユーザーのプレイ待ち

## 再開に必要なもの
- Visual Studio（MSVC v143）、Windows SDK
- Git Bash（`Tools/*.sh` 用）、Python と OpenCV（`Tools/remove_title_prompt.py` を再実行する場合だけ）
- `Document/test/` はテストのスクショ出力で、git に含めていない（`build_test.sh` で再生成される）
