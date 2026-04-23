
`radio/rfsimulator/simulator.cpp` には、RFsim内部で `path_loss_dB` を変更するための経路が追加されている。

* Low-level APIは `channel_desc_t` の `path_loss_dB` を直接更新し、旧値を返す。
* High-level APIは `rfsimulator_state_t`、`model_name`、`path_loss_dB` を受け取り、active channelを探索して一致したチャネルを更新する。
* Telnet I/Fである `rfsimu setpathloss <model_name> <path_loss_dB>` は、parseと表示だけを担当する。
* `chmod_file` / `chmod_poll_ms` は、RFsimが周期的にファイルを読み、接続済みactive channelにpathlossを反映するための設定である。

これらはRFsim内のchannel modelを動かす仕組みであり、USRPや実RUで送信電力、伝搬損失、アンテナ方向を変えるAPIではない。
