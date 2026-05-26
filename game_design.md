# Game Design: tinygame002

## 1. ゲームコンセプト (Concept)
- マルチプレイヤー（2人以上）の爆弾回しゲーム（ESP-NOW経由）
- プレイヤーは3つのスイッチのうち1つをクリックして爆弾を他のプレイヤーに回す
- スイッチの状態（ON/OFF）はプレイヤーには見えない（ブラインド操作）

## 2. 遊び方 (How to Play)
- 爆弾には3つの直列接続されたスイッチがある（起爆装置に接続）
- 初期状態では全スイッチがOFF
- スイッチの状態は累積される（前のプレイヤーがONにしたスイッチはONのまま）
- 全プレイヤー間で最新のスイッチ状態が共有される（ESP-NOWブロードキャスト）

### PLAY_MODEでの操作：
  - OLEDに3つのスイッチ番号を横一列に表示（**ON/OFF状態は非表示**）
  - 選択中のスイッチ番号は反転表示
  - Left/Rightキーでスイッチを選択
  - Downキーで選択中のスイッチをクリック（ONならOFF / OFFならON）
  - Buzzer: OFF
  - NeoPixels: Green
  - ALERT_STATUS: OFF

### スイッチクリック後の遷移：
  - クリックの結果、全スイッチがONになった場合 → **BOMBED_MODE**（自分が起爆 = 負け）
  - それ以外 → ALERT_STATUSをoffにする。全スイッチの状態をESP-NOWでブロードキャスト送信 → **WAITING_MODE**へ遷移

### WAITING_MODEでの動作：
  - 他のプレイヤーの誰か1人がスイッチをクリックするのを待つ
  - ESP-NOWでスイッチ状態を受信したら：
    - 全スイッチがONの場合 → **BOMBED_MODE**（他のプレイヤーが起爆 = 生存）
    - 残りOFFスイッチが1つの場合 → ALERT_STATUS = ON にして **PLAY_MODE**へ遷移
    - それ以外 → ALERT_STATUSをoffにする。**PLAY_MODE**へ遷移
  - ターン順は人間が管理（システムによる強制的な順番制御なし）

## 3. 操作方法 (Controls)
`config.h` で定義したボタンとの連動：
- **BUTTON_UP (D7):**
  - 短押し: No Action
  - 長押し（1.5秒以上）: **全状態で有効。ハードウェアリセット（再起動）を実行**
- **BUTTON_DOWN (D8):**
  - 短押し: スイッチ操作（ユーザーにはスイッチのステータスは見えない）（OnならOff/OffならOn）
- **BUTTON_LEFT (D9):**
  - 短押し: スイッチ番号DOWN（ループ）
  - 長押し: keep moving cursor (100msec interval)
- **BUTTON_RIGHT (D10):**
  - 短押し: スイッチ番号UP（ループ）
  - 長押し: keep moving cursor (100msec interval)

## 4. 画面表示 (OLED Display)
- **PLAY_MODE:**
  - スイッチ番号を横一列に表示（例: `[1] [2] [3]`）、選択中のものは反転表示
- **WAITING_MODE:**
  - "WAITING" をブリンク表示
- **BOMBED_MODE:**
  - 自分がクリックして爆発させた場合："You lose..."
  - 他の誰かが爆発させた場合："YOU SURVIVED"

## 5. NeoPixel
- **PLAY_MODE / WAITING_MODE:**
  - ALERT_STATUS = ON: Cyclic blink Amber (0.5s period)
  - ALERT_STATUS = OFF: All Green
- **BOMBED_MODE:**
  - 自分がクリックして爆発させた場合：All blink Red (0.5s period)
  - 他の誰かが爆発させた場合：All blink Green (0.5s period)

## 6. Buzzer
- モード切り替わり時：
  - BOMBED_MODEに切り替わった時：爆発音("Booom!")
  - それ以外の時："ピッ"
- ALERT_STATUS OFFからON時："ピピピッ！"

## 7. ゲームの状態 (Game States)
- **PLAY_MODE:** スイッチの一つを選択してクリックする。
- **WAITING_MODE:** 他のプレイヤーの誰かがスイッチをクリックする（ESP-NOW経由でスイッチ状態が届く）のを待つ。
- **BOMBED_MODE:** 自分を含む誰かが起爆させた。ゲームオーバー。BUTTON_UP長押しでリセット。

## 8. 技術メモ (Technical Notes)
- ハードウェアはtinygame001と同じ（ESP32-C3、OLED、ボタン4つ、ブザー、NeoPixel）
- tinygame001とは完全に別のゲーム。ソースコードは有用な部分があれば再利用する
- ESP-NOWはブロードキャストで全プレイヤーにスイッチ状態を送信
- **ネットワーク・パラメータ定義:**
  - `GAME_ID`: 他のESP-NOWデバイスとの混信を防ぐためのゲーム固有の識別子。送信データに含め、受信時に一致するパケットのみを処理する。
  - `WIFI_CHANNEL`: ESP-NOW通信で使用するWi-Fiチャンネル。全プレイヤーのデバイスで同じチャンネルを固定して使用する。
- 将来のオプション：BOMBED_MODEでBUTTON_UP短押しによる「もう一度プレイ」機能
