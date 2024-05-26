# m5stack-azure-iot
M5stack Core2 IoT開発キットにM5GO Bottom2を組み合わせたハードウエア(つまりM5Stack Core2 for AWS相当)を使って環境をAzure IoT にテレメトリを送信する。

対応しているセンサー  
- TVOC/eCO2 ガスセンサユニット(SGP30)
- Grove - SCD30搭載CO2温湿度センサー(Arduino用)
- M5Stack用温湿度気圧センサユニット Ver.3（ENV Ⅲ）
- M5Stack用 温湿度CO2センサ（SCD41）
- BME280

対応しているセンサーのどれか、または全部をPORT A(赤)に接続する。  

## 接続情報を用意する
WiFiとAzureIotHubの接続情報をjson形式で書いて`data/settings.json`ファイルに保存する。  
(AzureIotHubの接続情報は書かなくても動作するが)  

接続情報例(data/settings.json)
```
{
    "wifi": {
        "SSID": "************",
        "password": "************"
    },
    "AzureIoTHub": {
        "FQDN": "********************************",
        "DeviceID": "*************",
        "DeviceKey": "********************************************"
    }
}
```

## ファームウエアの書込み
M5StackCore2 + M5GO Bottom2 のセットまたは M5StackCore2 for AWS をUSB接続する。  
PlatformIO で Build & Upload する。

## 接続情報の書込み
PlatformIO で Upload Filesystem Image する。
