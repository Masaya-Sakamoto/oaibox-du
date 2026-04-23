
`beam_gains` と `tx_beam_gains` は、RFsim内でbeamごとの受信/送信利得差を簡易的に作るための検証用パラメータである。`run_beam_switch_test.sh` では、gNB側のChMod fileにpathlossと `tx_beam_gains` を書き込み、UEが観測するSSB/beam RSRPの優劣が変わるようにしている。

この仕組みは、MAC schedulerがCSI reportを見て `beam_selection_procedures()` を呼び、`UE_beam_index` を切り替えるまでの経路をRFsimで再現するためのものである。実機では、同じことをRFsim sample scalingではなく、実アンテナのbeam weight、RU制御、移動環境、可変attenuator、または測定環境で再現する必要がある。
