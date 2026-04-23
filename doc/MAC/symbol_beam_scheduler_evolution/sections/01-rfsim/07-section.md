
この資料の根拠にした主な確認範囲は以下である。

```bash
git diff 8259d8e459da46ea86a54090c0975c94b1a66c02..4c6eece66cacdac73147b950b5d262dc2e45fd85
git diff 4c6eece66cacdac73147b950b5d262dc2e45fd85..15063279788d6f14f8367a96008f431d26330a25
git log --reverse 4c6eece66cacdac73147b950b5d262dc2e45fd85..15063279788d6f14f8367a96008f431d26330a25
```

主な参照ファイルは以下である。

* [`gNB_scheduler_bch.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_bch.c): SIB1 TDA、Type0 CSS、SIB1 skip防御。
* [`gNB_scheduler_RA.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_RA.c): RA開始時のSSB/beam決定、Msg2/Msg4 scheduling。
* [`gNB_scheduler_primitives.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_primitives.c): beam allocation、FAPI beam変換、DCI RIV、beam selection。
* [`gNB_scheduler_uci.c`](../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_uci.c): CSI reportからのbeam selection、SR/PUCCH scheduling。
* [`beam_antenna_control.h`](../../openair2/LAYER2/NR_MAC_gNB/beam_antenna_control.h): antenna control callback境界。
* [`nr_ue_procedures.c`](../../openair2/LAYER2/NR_MAC_UE/nr_ue_procedures.c): UE PUCCH config guard。
* [`simulator.cpp`](../../radio/rfsimulator/simulator.cpp): RFsim ChMod、beam gains、pathloss、socket/beam処理。
* [`run_beam_switch_test.sh`](../../openair2/LAYER2/NR_MAC_gNB/tests/run_beam_switch_test.sh): RFsim統合テストの刺激生成。

凍結済みTDDテストとしては、以下の性質を固定している。

* `test_antenna_control.c`: default antenna control callbackとdestroy安全性。
* `test_beam_selection.c`: `UE_beam_index` 更新、old/new beam event、frame/slot伝搬、no-op条件。
* `test_beam_symbol_alloc.c`: beam allocationとresetの基本挙動。

RFsim統合テストでは、gNBログで以下を確認する。

* `Skipping SIB1 for SSB ... no valid TDA` が出てもgNBが継続する。
* `path_loss_dB: ... for channel 'rfsimu_channel_ue0'` または `tx_beam_gains: ...` がChMod fileから反映される。
* RA完了後、`Switching to beam` または `[AntennaCtrl] Beam switch` が出る。
* Telnet `Broken pipe` やstdin空コマンドにテスト成否が依存しない。
