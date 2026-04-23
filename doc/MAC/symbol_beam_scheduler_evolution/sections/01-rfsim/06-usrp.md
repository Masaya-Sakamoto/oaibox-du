
この章でいう実機移行は、RFsimをUSRPへ置き換えるだけの話ではない。想定する実験系はOTAであり、PC、USRP、アレイアンテナモジュールが協調して動作すること自体は既に成立している前提である。

今回のミソは、その既存のOTA制御系を、今回追加したMAC schedulerのbeam decisionと `antenna_control_if_t` に接続することである。RFsimでは `pathloss` や `tx_beam_gains` を人工的に変えてbeam switchを誘発したが、実機ではUEがOTAで観測するSSB/CSIの変化と、アレイアンテナモジュールへ投入するbeam table/位相設定が実際の制御対象になる。
