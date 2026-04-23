
実機移行時には、少なくとも以下を確認する。焦点は、MAC schedulerの判断をOTA制御系へ安全に渡す接続点である。

* `antenna_control_if_t.on_beam_switch` を、PC上のアレイアンテナ制御プロセスへ接続する。scheduler thread内で同期的に呼ばれるため、callback内でUSB/serial/socket I/Oを直接待たず、制御要求をqueueへ積んで別thread/processで処理する。
* `beam_switch_event_t.new_beam_index` とアレイアンテナモジュールのbeam tableを1対1または変換表で対応させる。MACのbeam index、FAPI beam ID、アンテナモジュールのbeam番号が同じとは限らないため、対応表を明示的に持つ。
* `event.frame` / `event.slot` と実アンテナ制御の反映時刻をログで相関できるようにする。OTAでは制御遅延があるため、「schedulerが決めたslot」と「アンテナが実際に切り替わった時刻」を分けて観測する。
* MACが設定するFAPI beam IDをPHY/RUが実際に消費しているかを確認する。ログ上のbeam switchだけでは、RFが向いているとは限らない。
* `LOPHY_BEAM_IDX` と通常beam indexの扱いが、対象USRP/RU構成のFAPI実装と一致するかを確認する。
* 64SSB、CORESET#0、searchSpaceZero、SIB1 TDAが、対象band/numerology/SSB bitmapで妥当かを確認する。
* RFsimの簡易gain変動で得たCSI/SSB RSRP変化と、実機UEが報告する測定周期、filtering、CSI report timingが一致しない可能性を前提に評価する。
* UE PUCCH guardはクラッシュ回避であり、実機運用ではPUCCH common configがいつ有効になるべきかを別途確認する。
