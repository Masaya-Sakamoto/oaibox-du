参照元 `beam_symbol_switch_2026_w15` は `2026.w15` 上の実験的実装であり、今回の移植後修正がすべて移植漏れだったわけではない。ここでは隣接ツリー `../beam_symbol_switch_2026_w15` の HEAD `4761812412` を基準に、`4c6eece6` 時点の移植直後実装と `15063279` 以降の修正内容を照合する。

| 領域 | 参照元での状態 | `4c6eece6` 時点の状態 | 判定 | 現在の整理 |
|---|---|---|---|---|
| SIB1 TDA / PDSCH beam 失敗 | `schedule_nr_sib1()` が SSB ごとに TDA と beam を選ぶが、TDA 不成立や SIB1 beam 不足を `AssertFatal()` で扱う。 | 参照元と同様に、一部 SSB の不成立が gNB 全体の fatal になりうる。 | 参照元由来 | `15063279` では SSB 単位で skip し、送信候補が全滅した場合だけ fatal とする整理に変更した。 |
| SIB1 BWP/RIV | 参照元にも global `cset0_bwp_size` と initial BWP 前提が残り、Type0 CSS の BWP と完全には分離されていない。 | Type0 CSS 対応を移植したが、RIV/DCI 幅の一部に initial BWP 前提が残った。 | 参照元由来 + 移植後調整不足 | Type0 CSS の `cset_start_rb` / `num_rbs` を使うように寄せ、SIB1 用 PDSCH/PDCCH BWP の局所不整合を解消した。 |
| RA BWP 構成順序 | PRACH 由来の SSB index から `UE_beam_index` を決めたあとに `configure_UE_BWP()` を呼ぶ。 | `configure_UE_BWP()` が beam 決定より前に呼ばれ、RA 用 BWP が同期 SSB/beam を見られない。 | 移植漏れ | 参照元の正しい順序に戻し、PRACH 由来 beam を RA context に反映してから BWP を構成する。 |
| `ssb_per_rach_occasion` の sixteen 対応 | 配列定義が `0.125` から `8` までで、`16` に対応する要素がない。 | 参照元と同じく `16` が未定義。境界条件で SSB index 計算が壊れる。 | 参照元由来 | `16` を明示的に追加し、設定値が sixteen の場合でも PRACH から SSB を決められるようにした。 |
| Msg3 未受信時の beam 解放 / RA release | WAIT_Msg3 が長く残った場合に、予約済み Msg3 beam を aging で解放する防御がない。 | 同様に RA proc が残り続け、後続 RA/SR/SIB1 の beam 競合を誘発しうる。 | 参照元由来の運用防御不足 | WAIT_Msg3 の経過 slot を見て RA proc を release し、`Msg3_beam` も reset する。 |
| SR / PUCCH beam allocation 失敗 | `nr_sr_reporting()` が SR 用 PUCCH beam 不足を `AssertFatal()` として扱う。 | 同様に SR の一時的な beam 競合で gNB が停止しうる。 | 参照元由来 | SR は UCI/PUCCH scheduling の資源競合として skip し、次 slot 以降の retry に委ねる。 |
| Beam switch 通知と frame/slot 伝搬 | beam switch は `UE_beam_index` 更新と RRC reconfiguration trigger に閉じており、外部 antenna control event や frame/slot 情報はない。 | RRC reconfiguration trigger も持たず、内部 beam index 更新に近い形で移植されていた。 | 新規要求 / 設計差分 + 移植差分 | `beam_switch_event_t` に frame/slot と old/new beam を持たせ、`antenna_control_if_t` に通知する境界を追加した。 |

この照合から、明確な移植漏れと言えるのは RA BWP 構成順序である。一方で、SIB1/SR の fatal、`ssb_per_rach_occasion` の `16` 欠落、Msg3 待ち RA proc の残留は参照元にも残っていた実験実装上の粗さであり、移植後の RFsim/USRP 検証で gNB 生存性の問題として顕在化したものと見なせる。
