# WeatherChecker

- configuration
  - WIFI_SSID、WIFI_PASS を WiFI 環境に合わせて設定
  - PROXY_HOST に Node-RED が稼働している環境に合わせて設定
  - PROXY_PORT は、Node-RED のポートを変えている場合は修正
- build
  - `pio run`
  - `.pio/build/pico/firmwware.uf2`を pico にファイルコピー
- proxy
  - `Node-RED`を使用し、`node-red\flows.json`を読み込む
