# Wi-FiスキャンからGoogle Maps上の現在位置を表示するための調査

調査日: 2026-09-21

Google公式の現行ドキュメントと規約だけを参照した。料金や規約は変更され得るため、公開前にリンク先を再確認すること。

## 結論

技術的には、ESP32/M5Stackで周辺アクセスポイントをスキャンし、BSSIDとRSSIをGoogle Maps Platform Geolocation APIへ送ると、推定緯度・経度と精度半径を得られる。その座標をMaps Static APIの`center`と`markers`へ渡せば、Google Mapsの静止画像として端末に表示できる。

本番向けの推奨構成は次のとおり。

1. 端末が周辺Wi-Fiをスキャンする。
2. 端末はBSSID、RSSI、チャンネルを認証付き自前プロキシへHTTPSで送る。
3. プロキシがGeolocation APIを呼び、`lat`、`lng`、`accuracy`を端末へ返す。
4. プロキシがMaps Static APIの署名済みURLを生成する。
5. 端末はGoogleから地図画像をHTTPSで取得し、帰属表示を含む画像全体を表示する。
6. BSSID、座標、地図画像は原則として永続保存しない。

APIキーやURL署名secretをファームウェアへ埋め込む構成は避ける。GoogleはWeb ServiceのAPIキーを信頼できない端末へ公開しないよう求め、クライアントから直接呼ぶ必要がある場合にも安全なプロキシを案内している。Maps Static APIの署名もサーバー側で行う必要がある。

- [Google Maps Platform security guidance](https://developers.google.com/maps/api-security-best-practices)
- [Maps Static API: Use a Digital Signature](https://developers.google.com/maps/documentation/maps-static/digital-signature)

## Geolocation API

### リクエスト

エンドポイントは次のHTTPS POST。

```text
https://www.googleapis.com/geolocation/v1/geolocate?key=YOUR_API_KEY
```

`Content-Type: application/json`でJSONを送る。Wi-Fiだけで位置を求める最小例は次の形になる。

```json
{
  "considerIp": false,
  "wifiAccessPoints": [
    {
      "macAddress": "f0:d5:bf:fd:12:ae",
      "signalStrength": -43,
      "channel": 11,
      "age": 0
    },
    {
      "macAddress": "30:86:2d:c4:29:d0",
      "signalStrength": -58,
      "channel": 6,
      "age": 0
    }
  ]
}
```

`considerIp`の既定値は`true`。Wi-Fi情報が不足すると送信元IPアドレスによる推定へフォールバックする。Wi-Fiスキャンによる位置だけを採用したい場合は`false`にする。IP推定は精度半径が数千メートルになる場合があり、`false`で404になるならアクセスポイントを位置特定できていないと判断できる。

- [Geolocation request and response](https://developers.google.com/maps/documentation/geolocation/requests-geolocation)
- [Geolocation API troubleshooting](https://developers.google.com/maps/documentation/geolocation/requests-geolocation-errors)

### Wi-Fiアクセスポイント項目

`wifiAccessPoints`には、物理的に異なる固定アクセスポイントを2個以上含める必要がある。2個未満では成功せず、404 `notFound`になり得る。列車や飛行機など移動するアクセスポイントはサービス側で無視される。

| 項目 | 必須 | JSON型 | 内容 |
| --- | --- | --- | --- |
| `macAddress` | 必須 | string | Wi-FiノードのBSSID/MAC。コロン区切り16進表記 |
| `signalStrength` | 任意・推奨 | double | dBm単位のRSSI。通常は負値。文書上の範囲は-128〜-10 dBmで、-10 dBmより大きい値は`NOT FOUND` |
| `age` | 任意 | uint32 | 検出からの経過ミリ秒 |
| `channel` | 任意 | uint32 | Wi-Fiチャンネル |
| `signalToNoiseRatio` | 任意 | double | dB単位の信号対雑音比 |

SSIDを送る項目はない。ESP32のスキャン結果から使うのはBSSID、RSSI、チャンネルであり、SSIDやWi-Fiパスワードは送らない。

Googleが位置特定に使えるのはuniversally administered MAC addressだけで、locally administered MAC addressは黙って破棄される。少なくとも次を事前に除外する。

- ブロードキャストアドレス`FF:FF:FF:FF:FF:FF`
- IANA予約範囲`00:00:5E:00:00:00`〜`00:00:5E:FF:FF:FF`
- locally administered bitが立ったMACアドレス

フィルター後に2個以上残ることを確認する。位置精度はアクセスポイントの数、密度、信号強度で変化し、Googleの目安では2個以上のWi-Fi APを含む場合の精度半径は通常約20mだが、保証値ではない。

- [Geolocation request and response: WiFi access point objects](https://developers.google.com/maps/documentation/geolocation/requests-geolocation#wifi_access_point_objects)
- [Geolocation request and response: Dropping unused WiFi access points](https://developers.google.com/maps/documentation/geolocation/requests-geolocation#dropping_unused_wifi_access_points)

### レスポンス

成功時はJSONで座標と精度半径が返る。

```json
{
  "location": {
    "lat": 35.681236,
    "lng": 139.767125
  },
  "accuracy": 120
}
```

- `location.lat`: 推定緯度（度）
- `location.lng`: 推定経度（度）
- `accuracy`: 推定点を中心とした精度円の半径（メートル）

UIには「現在地」と断定せず、精度半径とともに「推定位置」と表示するのが適切。代表的なエラーは400 `keyInvalid` / `parseError`、403 `dailyLimitExceeded` / `userRateLimitExceeded`、404 `notFound`。

- [Geolocation responses](https://developers.google.com/maps/documentation/geolocation/requests-geolocation#geolocation_responses)
- [Geolocation API errors](https://developers.google.com/maps/documentation/geolocation/requests-geolocation-errors#error_descriptions)

## Maps Static API

### URL

基本URLは次のとおり。

```text
https://maps.googleapis.com/maps/api/staticmap?parameters
```

取得した位置へマーカーを置く例:

```text
https://maps.googleapis.com/maps/api/staticmap?center=35.681236,139.767125&zoom=15&size=320x240&format=jpg-baseline&maptype=roadmap&markers=color:red%7Clabel:P%7C35.681236,139.767125&key=YOUR_API_KEY&signature=YOUR_SIGNATURE
```

位置情報と認証情報を含むためHTTPSを使う。`|`などの予約文字はURLエンコードし、署名する場合はエンコード後のURLへ署名する。

- [Maps Static API: Get Started](https://developers.google.com/maps/documentation/maps-static/start)
- [Static Web API best practices](https://developers.google.com/maps/documentation/maps-static/static-web-api-best-practices)

### 主な制約

- `size={width}x{height}`は必須。標準上限は`640x640`。
- `scale`は`1`または`2`。`size=640x640&scale=2`は1280x1280ピクセルを返すが、表示範囲は変わらない。
- URL全体は16,384文字まで。
- `center`と`zoom`は通常必須。ただし`markers`、`path`または`visible`から表示範囲を決めさせる場合は省略できる。
- 緯度・経度は小数点以下6桁までが使用され、それを超える精度は無視される。
- `maptype`は`roadmap`（既定）、`satellite`、`terrain`、`hybrid`。
- `format`は`png8` / `png`（既定）、`png32`、`gif`、`jpg`、`jpg-baseline`。

M5Stack側のJPEGデコーダー互換性とRAM消費を考えると、非プログレッシブJPEGである`jpg-baseline`を最初の候補にできる。これはGoogleの形式仕様に基づく実装上の選択であり、画質と画像サイズは実機で確認する。

### マーカー

形式は次のとおり。区切り文字`|`は送信時に`%7C`へエンコードする。

```text
markers=markerStyles|markerLocation1|markerLocation2
```

- `size:` は`tiny`、`mid`、`small`、または省略時の通常サイズ。
- `color:` は24-bit色または定義済み色。
- `label:` は大文字英数字1文字。ラベルを表示できるのは通常サイズと`mid`だけ。
- 同じスタイルなら1個の`markers`へ複数地点を列挙できる。異なるスタイルは`markers`を複数指定する。
- `markers`だけで自動的に中心とズームを決めることもできる。

- [Maps Static API markers](https://developers.google.com/maps/documentation/maps-static/start#Markers)

### 認証、請求、制限

Geolocation APIとMaps Static APIの両方について、Google Cloudプロジェクトで請求先を有効化し、各APIを有効化する。どちらも従量課金であり、正確な料金は現行価格表を確認する。日次クォータとアラートを設定して費用を制御できる。

- Geolocation APIのガイド上のリクエストは`key`クエリパラメータを必須としている。
- Maps Static APIの`key`は必須、`signature`は推奨。署名なしリクエストは利用可能クォータが制限され、失敗する場合がある。
- APIキーにはAPI restrictionを付け、必要なAPI以外では使えないようにする。
- アプリごと・用途ごとにキーを分け、未使用APIをプロジェクトで無効にする。
- 固定IPのプロキシからWeb Serviceを呼ぶ場合はIP address application restrictionを使う。
- ESP32が接続するWi-FiのグローバルIPは通常変動するため、端末から直接呼ぶキーをIP制限する運用は実用的でない。
- URL署名secretは端末やソースツリーへ置かず、プロキシだけで保持する。
- Maps Static APIはサーバーで署名URLを生成し、端末へ返すかGoogleへリダイレクトする。署名なしリクエストのクォータを必要最小限、可能なら0にする。

- [Geolocation API usage and billing](https://developers.google.com/maps/documentation/geolocation/usage-and-billing)
- [Maps Static API usage and billing](https://developers.google.com/maps/documentation/maps-static/usage-and-billing)
- [Set up the Maps Static API](https://developers.google.com/maps/documentation/maps-static/get-api-key)
- [Google Maps Platform security guidance](https://developers.google.com/maps/api-security-best-practices)

## Attribution、キャッシュ、保存

### Attribution

Geolocation APIの結果を地図上に出す場合はGoogle Map上に表示する必要がある。本アプリでMaps Static API画像へ表示する構成はこの条件を満たす。

Googleが地図画像に含めたロゴ、著作権表示、第三者データ提供者表示を削除、変更、隠蔽してはならない。端末では次を守る。

- Static Maps画像をクロップせず、画像全体を表示する。
- 画面下端のロゴや権利表示へステータスバー、ボタン、精度表示などを重ねない。
- 縮小によって帰属表示が判読不能にならないようにする。
- 幅180ピクセル未満の地図では小型Googleロゴが使われることを考慮する。

Google Mapに含まれる帰属表示が見える場合、Geolocation結果に追加の帰属表示は不要。座標や精度などを地図なしで表示する画面では、原則として公式Google Mapsロゴ、スペースが限られる場合は正確な文字列`Google Maps`を表示する。テキストは翻訳や改行をせず、12〜16sp、通常ウェイト、指定色と十分なコントラストで表示する。

- [Geolocation API policies and attribution](https://developers.google.com/maps/documentation/geolocation/policies)
- [Google Maps Platform Terms of Service](https://cloud.google.com/maps-platform/terms)

### Cache / storage

現行のService Specific Termsでは、Geolocation APIが返した`lat` / `lng`は最大30暦日まで一時キャッシュでき、その後は削除しなければならない。現在位置表示だけなら永続化せず、RAM上で現在セッションだけ保持するのが簡潔で安全。

一般規約はGoogle Maps Contentのpre-fetch、index、store、reshare、rehostと、個別規約で許可されていないキャッシュを禁止している。Maps Static API画像には個別のキャッシュ例外がないため、保守的な実装は次のとおり。

- 地図画像をFlash、SD、NVSへ保存しない。
- 過去の地図をオフライン再利用しない。
- 表示のたびにGoogleから取得し、デコード中・表示中のRAMだけで扱う。
- 自前サーバーへ画像を保存して再配信しない。

GoogleのFAQも、Static Maps画像を保存して自前サイトから配信せず、Google Maps APIが生成する画像をユーザーへ直接提供するよう案内している。

- [Current Google Maps Platform Service Specific Terms: Geolocation API](https://cloud.google.com/maps-platform/terms/maps-service-terms)
- [Google Maps Platform Terms: License restrictions](https://cloud.google.com/maps-platform/terms)
- [Google Maps Platform FAQ](https://developers.google.com/maps/faq)

## プライバシーと同意

周辺APのBSSIDは位置推定の入力そのものである。Google Maps Platform規約は、位置を取得する前に、収集するデータの種類と、位置情報を他社データと組み合わせる場合の用途を通知し、事前・明示的・撤回可能な同意を得るよう求めている。Googleはサービス提供・改善のためにIPアドレスや緯度・経度などを受領、収集、利用、保持し得る。

実装時の最低要件:

- 初回取得前に「周辺Wi-FiのBSSID、信号強度、チャンネルおよび送信元IPをGoogleへ送信し、概算位置を取得する」と明示する。
- 同意する操作があるまでスキャン結果を送信しない。
- 設定から同意を撤回でき、撤回後は位置取得と送信を止める。
- BSSID、RSSI、位置、Static Maps URLをSerialログ、クラッシュログ、アクセスログへ残さない。プロキシでもリクエストbodyとクエリのログを抑制する。
- ユーザーID、氏名、メールアドレス、端末シリアルなどをGeolocationリクエストに結び付けない。
- SSIDとWi-Fiパスワードは収集も送信もしない。
- 位置取得中、最終更新時刻、精度半径をUIに示す。
- アプリの利用規約にGoogle Maps機能/コンテンツを含むことと、Google Maps End User Additional TermsおよびGoogle Privacy Policyが適用されることを記載する。
- プライバシーポリシーで収集・送信項目、目的、保存期間、撤回方法を説明する。

規約上、位置情報取得の同意が必要である一方、アクセスポイント情報を保存しないことはデータ最小化のための実装推奨である。

- [Google Maps Platform Terms: Terms, privacy and end-user location](https://cloud.google.com/maps-platform/terms)
- [Google Maps End User Additional Terms](https://maps.google.com/help/terms_maps/)
- [Google Privacy Policy](https://policies.google.com/privacy)

## 実装チェックリスト

- [ ] Google Cloudプロジェクトでbillingを有効化
- [ ] Geolocation APIを有効化
- [ ] Maps Static APIを有効化
- [ ] プロキシ用キーをAPI制限・IP制限
- [ ] Maps Static URLをサーバー側で署名
- [ ] unsigned Static Maps quotaを制限
- [ ] 位置取得前の明示同意と撤回UI
- [ ] Wi-Fi APをフィルターし、有効な異なる固定APを2個以上送信
- [ ] Wi-Fi由来だけを必要とする場合は`considerIp: false`
- [ ] `accuracy`を位置と同時に表示
- [ ] 404、認証エラー、クォータ超過、通信失敗のUI
- [ ] APIキー、署名secret、BSSID、位置をログへ出さない
- [ ] 地図画像を永続キャッシュしない
- [ ] 地図画像の帰属表示をクロップ・上書きしない
- [ ] 利用規約・プライバシーポリシーを用意

## EEAについて

適用規約は端末の現在地ではなくGoogle Maps Platform請求先住所で分かれる。請求先がEEA内ならEEA TermsとEEA Service Specific Termsを別途確認する。この文書の規約要約は、主にEEA外の現行規約を前提としている。

- [Google Maps Platform EEA Terms](https://cloud.google.com/maps-platform/terms/eea)
- [Google Maps Platform EEA Service Specific Terms](https://cloud.google.com/terms/maps-platform/eea/maps-service-terms)
