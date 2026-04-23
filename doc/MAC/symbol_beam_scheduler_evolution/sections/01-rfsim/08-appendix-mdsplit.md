
この資料は、`mdsplit` の分解成果物を原本として管理する。単一Markdownの `doc/MAC/symbol_beam_scheduler_evolution.md` は、原本から再構成した表示・レビュー用の成果物である。

```bash
mdsplit verify doc/MAC/symbol_beam_scheduler_evolution/hierarchy.json
mdsplit compose doc/MAC/symbol_beam_scheduler_evolution/hierarchy.json -o doc/MAC/symbol_beam_scheduler_evolution.md
```

`doc/MAC/symbol_beam_scheduler_evolution/sections/` 以下では、章ごとにMarkdownを確認できる。`hierarchy.json` には見出し階層、section file、順序がJSONとして保存される。本文を編集する場合は原則としてsection fileを更新し、`mdsplit compose` で単一Markdownへ反映する。

compose結果を確認するだけなら、以下のように `/tmp` へ出力して差分を見る。

```bash
mdsplit compose doc/MAC/symbol_beam_scheduler_evolution/hierarchy.json -o /tmp/symbol_beam_scheduler_evolution.md
diff -u /tmp/symbol_beam_scheduler_evolution.md doc/MAC/symbol_beam_scheduler_evolution.md
```

分解原本と単一Markdownのどちらか一方だけを変更すると内容がずれるため、レビュー時には両方が同じ内容を表していることを確認する。
