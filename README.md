# DirectX12_3D — First Sword（リマスター版）
学生が DirectX12 で作成した 3D アクションを、グラフィックス・ゲーム性・UI の面から作り直したものです。

## 主な変更点
### グラフィックス（`FrameWork/Graphics/PostProcess/SceneRenderer.cpp`）
- HDR レンダリング（R16G16B16A16_FLOAT）と ACES トーンマップ
- 2048² シャドウマップ。PCF（16 タップの Poisson、回転付き）とテクセルスナップ
- PBR（GGX / Smith / Schlick）と、スカイボックスを使った IBL・リムライト
- SSAO、Bloom（CoD:AW 方式の 13 タップ縮小と、テント拡大の 6 段）、高さフォグ＋太陽方向の散乱
- カラーグレーディング、ビネット、フィルムグレイン、FXAA
- 地形は Poly Haven の月面 PBR テクスチャ（CC0、albedo / normal / ARM）をワールド座標で貼り、大きなスケールで明度に変化を付けている
- テクスチャのミップマップを自動生成し、異方性フィルタを使用

### ゲーム性（`Source/Application/Combat/`）
- パリィ（押してから 0.18 秒が受付）、ガード（長押し）、ガード崩し
- 敵の攻撃は「予備動作 → 予告の閃光 → 攻撃判定 → 硬直」。武器の光り方で読める。ボスは弾けない攻撃（赤）も使う
- 体幹ゲージ：満タンにすると体幹崩し（スタンし、被ダメージ 2.5 倍）
- スタミナ（回避とガードで消費）
- ヒットストップ、スロー、カメラ揺れ（`GameFeel`）
- エフェクトは Effekseer をやめて自作のパーティクルに置き換えた（`FrameWork/Graphics/Particles`）。HDR で発光する火花・閃光・衝撃波・剣の軌跡

### UI（`Source/Application/UI/Hud.cpp`）
- 体力（減った分を白い残像で表示）、スタミナ、敵の名前・体力・体幹、スキルのクールダウン、操作ガイド
- ダメージ数値、PARRY / BREAK などの表示、攻撃予告マーク（`!` / `危`）、被弾時に画面の縁が赤くなる

### チュートリアル（`Source/Application/Scene/Tutorial/`）
ステージ選択画面の「修練の木人」に近づいて F キーを押すと開始します。移動 → カメラ → 攻撃 → 回避 → ガード → パリィ → 体幹崩し → スキル → 卒業試験、の 9 段階です。

設計の詳細は [`Document/GameDesign.md`](Document/GameDesign.md) にあります。

## 操作
| 操作 | キーボード / マウス | パッド |
|---|---|---|
| 移動 | WASD | 左スティック |
| カメラ | マウス | 右スティック |
| 攻撃 | 左クリック | R2 |
| パリィ / ガード（長押し） | 右クリック / C | L2 |
| 回避 | Shift | A |
| スキル | Q / E | L1 / R1 |
| 決定（ステージ選択） | F | X |

## ビルドとテスト
```bash
bash Tools/build_test.sh            # Debug と Release のビルド、単体テスト、全シーンのスモークテスト
bash Tools/build_test.sh Release    # Release だけ
bash Tests/run_unit_tests.sh        # 戦闘ルールの単体テストだけ
```
スモークテストでは各シーンを固定フレーム数だけ自動で実行し、次の 3 点を確認します。
- 異常終了しないこと
- ログにエラーが出ないこと
- 最終フレームのスクリーンショットが保存されること（`Document/test/`）

`--autoplay` を付けると、自動入力で戦闘とチュートリアルを進めます。

### 起動引数
| 引数 | 内容 |
|---|---|
| `--scene N` | 開始シーン（0:タイトル 1:ステージ選択 2:戦闘 3:クリア 4:ゲームオーバー 5:チュートリアル） |
| `--enemy N` | 戦闘の敵（0:蒼 1:金 2:紅） |
| `--frames N` | N フレームで終了する（固定ステップで実行） |
| `--capture PATH` | 終了前のフレームを PNG で保存する |
| `--autoplay` | 自動入力 |
| `--sun x,y,z` | 太陽の方向を上書きする（ライティングの確認用） |

## 使用素材
- 月面テクスチャ：Poly Haven「moon_02」（CC0）https://polyhaven.com/a/moon_02
- フォント：Noto Sans JP（SIL Open Font License）
