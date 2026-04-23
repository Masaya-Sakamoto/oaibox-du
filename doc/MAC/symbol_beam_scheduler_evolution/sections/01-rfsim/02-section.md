
| Commit | 位置づけ | スケジューラ上の意味 | RFsim上の意味 | 残った前提 |
|---|---|---|---|---|
| `8259d8e` | 開発起点。OpenAirInterface 5G `2026.w07` をベースに、AllbesmartがO-DU端末向けにカスタムしたもの。 | 既存OAI schedulerの挙動をベースにする。MAC schedulerの中心責務はPF scheduling、RA、SIB1、UCIなどの通常資源割当であり、ビームごとのシンボル資源競合は主要な抽象として扱っていない。 | RFsimは通常の接続・チャネルモデル検証用。64SSBのビーム切替刺激を作る専用経路はない。 | 実機/O-DU向けの開発ベースであり、今回のビーム切替検証用の前提はまだ入っていない。 |
| `4c6eece6` | OAI upstreamで試作していたMAC層シンボル単位ビームスケジューラを半機械的に移植したもの。 | `NR_beam_info_t`、beam period、beam duration、per-beam VRB map、`beam_allocation_procedure()` などにより、schedulerがbeam resourceを明示的に予約するようになる。 | FR1 RFsim向けのbeam-symbol設定と単体/統合テストの土台が入る。 | 上流試作の前提が残り、64SSB SAでのSIB1/RA/PUCCH失敗を通常運用として扱う防御が不足している。 |
| `15063279` | RFSIM上で64SSB定義および簡易チャネル変動に伴うビーム切替スケジューリングを達成したもの。 | SIB1、RA、SR、PUCCH、CSI/TCI timerの各経路で、64SSBとsymbol-level beam allocationに耐えるための修正が入る。`UE_beam_index` 更新時に `antenna_control_if_t` へ通知できる。 | `chmod_file`、`rfsimu setpathloss`、`beam_gains`、`tx_beam_gains`、RFsim beam read/write補正により、チャネル変動とビーム切替をRFsim内で注入できる。 | RFsimの刺激生成は実機RF制御ではない。USRP/RUでは実アンテナ制御API、測定環境、beam ID伝搬の確認が別途必要になる。 |
