
以下はRFsim検証用の仕組みであり、OTA実機では置き換えが必要である。

* `chmod_file` pollingによるpathloss変更。
* `rfsimu setpathloss` によるchannel model操作。
* `beam_gains` / `tx_beam_gains` によるsample scaling。
* `run_beam_switch_test.sh` のRFsim ChMod file注入シナリオ。

実機では、これらをアレイアンテナモジュール制御、USRP RF経路、OTAで実際に発生するチャネル変動へ置き換える。つまり、`path_loss_dB` をファイルで書き換えるのではなく、UEがOTAで観測するSSB/CSIの変化を入力にし、MACが選んだbeamをアレイアンテナモジュールのbeam tableまたは位相設定へ反映する。

`antenna_control_if_t` のdefault loggerは実機制御ではない。実機では `on_beam_switch` の実装を差し替え、`beam_switch_event_t.new_beam_index` をPC側のアンテナ制御コマンドへ変換する必要がある。USRP自体はRF sampleの送受信経路を担い、beam方向の実制御はアレイアンテナモジュール側の既存制御APIに接続する、という責務分担で整理する。
