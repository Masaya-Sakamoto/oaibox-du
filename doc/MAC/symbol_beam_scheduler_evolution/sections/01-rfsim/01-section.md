
`4c6eece6` は、上流で試作されていたシンボル単位ビームスケジューラを開発ベースへ移植し、MAC schedulerがビームごとのシンボル資源を管理する段階まで進めたコミットである。この時点で、PDCCH/PDSCH/PUCCH/PUSCH/SIB1/RAは `beam_allocation_procedure()` を通じて同一slot内のbeam resourceを取り合うようになった。

一方で、`4c6eece6` の実装は「移植された資源モデル」としては成立していても、FR2 64SSBのSA RFsim統合シナリオで必要になる防御が不足していた。具体的には、一部SSBでSIB1 TDAが取れないケース、RA時にUE beamが決まる前にBWPを構成するケース、PUCCH/SRのbeam allocation失敗をfatalにするケースなどが残っていた。

`15063279` は、64SSB + SA + RFsim上の簡易チャネル変動で、gNB/UEを落とさずRA完了とビーム切替スケジューリングへ到達するための補正を入れた段階である。scheduler本体の修正と、RFsimでチャネル状態を人工的に変えるための仕組みが同じ到達点に含まれているため、実機移行時には両者を分けて扱う必要がある。
