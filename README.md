# Xteink-shuffler

Xteink X4 を **電子ペーパー・カード引きマシン** にするカスタムファームウェアと、カード画像生成ツールです。

## できること

| 操作 | ボタン | 動作 |
|------|--------|------|
| シャッフル（表紙） | **Back**（ボタン1） | 現在デッキの `cover.bmp` を表示 |
| カードを引く | **Confirm / Left / Right**（ボタン2〜4） | 現在デッキからランダムに1枚表示 |
| デッキ切替 | **Up / Down**（上下） | SDカード上の別フォルダへ |

電源ボタンを **1秒以上** 長押しするとスリープします。

## SDカードのフォルダ構成

FAT32 の microSD ルートに、デッキごとにフォルダを作ります。

```text
/aaa/cover.bmp    ← 表紙
/aaa/000.bmp      ← カード
/aaa/001.bmp
/aaa/002.bmp
/bbb/cover.bmp    ← 別デッキ
/bbb/000.bmp
```

- 画像サイズ: **480×800** ピクセル（縦向き）
- 形式: **1-bit BMP**（白黒）推奨
- カードファイル名: `000.bmp` 〜 `999.bmp`（3桁 + `.bmp`）

## ファームウェアの書き込み

### 方法A: ブラウザから（おすすめ）

1. X4 を USB-C で PC に接続し、電源を入れる
2. [https://xteink.dve.al/](https://xteink.dve.al/) を開く
3. ビルド済み `firmware/.pio/build/esp32-c3-devkitm-1/firmware.bin` をアップロード

> 元の公式ファームウェアに戻す前に、必ずバックアップを取ってください。

### 方法B: PlatformIO

```bash
cd firmware
pio run --target upload
```

ビルドのみ:

```bash
cd firmware
pio run
```

## カード画像の作り方

### シェルコマンド（PC）

```bash
chmod +x tools/generate-cards.sh
./tools/generate-cards.sh examples/cards.txt ./sd/aaa --font /path/to/font.ttf
```

生成物を SD カードの `/aaa/` にコピーします。

オプション:

- `--font-size 42` フォントサイズ
- `--cover-title "おみくじ"` 表紙タイトル

### ブラウザ上で作成

`web/index.html` をブラウザで開きます。

1. カード文言を入力（1行1枚）
2. フォント（TTF/OTF）を選択
3. プレビューで確認
4. **ZIPをダウンロード** → 解凍して SD カードへコピー

ローカルサーバーで開く例:

```bash
cd web && python3 -m http.server 8080
# http://localhost:8080 を開く
```

## ボタン配置（X4）

```text
前面（左→右）: Left | Confirm | Right | Back
側面: Up / Down
電源: Power（側面）
```

本ファームウェアの割り当て:

- **Back** = 表紙（シャッフル）
- **Confirm / Left / Right** = ランダム抽選
- **Up / Down** = デッキ選択

## プロジェクト構成

```text
firmware/          ESP32-C3 ファームウェア (PlatformIO)
tools/             カード BMP 生成スクリプト
web/               ブラウザ版カード生成
examples/          サンプル文言リスト
```

## ライセンス

MIT

## 参考

- [CidVonHighwind/xteink-x4-sample](https://github.com/CidVonHighwind/xteink-x4-sample) — ハードウェア定義の参考
- [ZinggJM/GxEPD2](https://github.com/ZinggJM/GxEPD2) — ディスプレイドライバ
