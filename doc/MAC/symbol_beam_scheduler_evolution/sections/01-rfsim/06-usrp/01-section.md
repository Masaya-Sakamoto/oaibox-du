
以下はRFsimだけの回避ではなく、OTAでも必要なMAC decisionの安定化として維持すべき修正である。実機ではRFsimよりも測定遅延、RF制御遅延、OTAチャネル変動が大きくなるため、schedulerが「一部の資源が置けない」状態でgNB全体を落とさないことがより重要になる。

* 64SSB時に一部SSBのSIB1 TDA失敗をgNB全体のfatalにしない防御。
* SIB1/RA/SI-RNTI/RA-RNTIのBWP size/start/RIVを対象BWPに合わせる修正。
* RA開始時にPRACH由来のSSB/beamを決めてからUE BWPを構成する順序。
* SR/PUCCH用beam allocation失敗を通常の資源競合として扱う修正。
* `UE_beam_index` 更新を `antenna_control_if_t` へ通知する境界。

これらは、RFsimで顕在化したとしても、実機の64SSB/analog beamforming構成でも維持すべきである。特に `antenna_control_if_t` は、MACが選んだ `new_beam_index` をPC側のアンテナ制御プロセスへ渡すための最小境界として扱う。
