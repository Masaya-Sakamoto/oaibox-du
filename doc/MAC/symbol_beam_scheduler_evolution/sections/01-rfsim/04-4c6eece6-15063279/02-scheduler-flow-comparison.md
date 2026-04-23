ここでは、参照元の実験的な流れ、`15063279` 以降の防御付き実装、本来あるべき責務分離の3段階を並べる。3つ目は今回実装した差分そのものではなく、調査から見えた scheduler 構成の整理案である。

#### 移植元 `beam_symbol_switch_2026_w15` の実験的な流れ

参照元では channel-specific scheduler が必要な場面で直接 `beam_allocation_procedure()` を呼び、beam が取れない場合は多くの経路で fatal または即 return になる。SIB1 scheduler は SSB ごとの Type0 CSS/TDA/beam を扱うが、一部 SSB の不成立を通常の scheduling miss として扱わない。RA scheduler は PRACH 由来 beam を BWP 構成前に決める点は正しいが、beam switch は `UE_beam_index` 更新と RRC reconfiguration trigger 寄りで、外部アンテナ制御との明示的な境界を持たない。

```mermaid
sequenceDiagram
  participant Tick as Slot tick
  participant SIB1 as SIB1 scheduler
  participant RA as RA scheduler
  participant SR as SR/PUCCH scheduler
  participant Alloc as beam_allocation_procedure
  participant Switch as beam_switching_procedure
  participant UE as UE state / RRC

  Tick->>SIB1: schedule_nr_sib1()
  SIB1->>Alloc: reserve PDCCH beam
  alt no PDCCH beam
    SIB1-->>Tick: AssertFatal
  else PDCCH beam reserved
    SIB1->>SIB1: select Type0 CSS and TDA
    alt no valid TDA for this SSB
      SIB1-->>Tick: AssertFatal
    else TDA exists
      SIB1->>Alloc: reserve PDSCH beam
      alt no PDSCH beam
        SIB1-->>Tick: AssertFatal
      else SIB1 scheduled
        SIB1-->>Tick: emit SIB1 DL PDUs
      end
    end
  end

  Tick->>RA: PRACH detected
  RA->>RA: derive SSB and beam from PRACH
  RA->>RA: configure_UE_BWP()
  RA->>Alloc: reserve Msg2 / Msg3 / Msg4 beams

  Tick->>SR: nr_sr_reporting()
  SR->>Alloc: reserve PUCCH beam for SR
  alt no PUCCH beam
    SR-->>Tick: AssertFatal
  else SR scheduled
    SR-->>Tick: emit PUCCH SR
  end

  Tick->>Switch: beam decision
  Switch->>UE: update UE_beam_index
  Switch->>UE: trigger RRC reconfiguration
  Note over SIB1,SR: Ordinary beam contention can cross into process fatal paths.
```

#### `15063279` 以降の防御付きの流れ

移植後修正では、SIB1 の候補 SSB 単位 skip、RA の beam 決定後 BWP 構成、SR の beam 不足 skip、beam switch event の外部通知を追加した。これにより、beam availability の一時的な不足を scheduler の通常の失敗として扱える範囲が広がった。

```mermaid
sequenceDiagram
  participant Tick as Slot tick
  participant SIB1 as SIB1 scheduler
  participant RA as RA scheduler
  participant SR as SR/PUCCH scheduler
  participant Alloc as beam_allocation_procedure
  participant Switch as beam switch decision
  participant Ant as antenna_control_if_t

  Tick->>SIB1: schedule_nr_sib1()
  loop active SSB candidates
    SIB1->>SIB1: resolve Type0 CSS BWP and TDA
    alt no valid TDA or BWP for candidate
      SIB1-->>SIB1: skip this SSB
    else TDA/BWP valid
      SIB1->>Alloc: reserve PDCCH and PDSCH beams
      alt beam unavailable
        SIB1-->>SIB1: reset reservation and skip this SSB
      else beam reserved
        SIB1-->>Tick: emit SIB1 DL PDUs
      end
    end
  end
  alt all attempted SSBs failed
    SIB1-->>Tick: fatal configuration failure
  end

  Tick->>RA: PRACH detected
  RA->>RA: derive SSB and beam from PRACH
  RA->>RA: configure_UE_BWP() after beam selection
  RA->>Alloc: reserve RA beams
  opt WAIT_Msg3 ages out
    RA->>Alloc: release Msg3 beam
    RA-->>Tick: release RA proc
  end

  Tick->>SR: nr_sr_reporting()
  SR->>Alloc: reserve PUCCH beam for SR
  alt beam unavailable
    SR-->>Tick: skip SR in this slot
  else SR scheduled
    SR-->>Tick: emit PUCCH SR
  end

  Tick->>Switch: beam switch decision with frame/slot
  Switch->>Ant: notify beam_switch_event_t
  Ant-->>Switch: accepted by adapter boundary
```

#### 本来あるべき scheduler 構成

本来の構成では、`beam_allocation_procedure()` は scheduler 共通の beam resource allocator として維持し、SIB1/RA/DL/UL/UCI scheduler はそれぞれの channel 仕様、TDA、HARQ、RB/CCE/PUCCH 選択に集中するのが望ましい。beam availability の不足は原則として「その候補を skip / retry」扱いにし、設定矛盾や全候補不成立だけを fatal とする。外部アンテナ制御は scheduler thread から重い I/O を直接実行せず、`antenna_control_if_t` を queue 投入または非同期 adapter の境界として扱う。

```mermaid
sequenceDiagram
  participant Tick as Slot tick
  participant Sched as SIB1/RA/DL/UL/UCI schedulers
  participant Helper as BWP/TDA/HARQ helpers
  participant Alloc as common beam allocator
  participant Policy as beam policy
  participant Events as event queue
  participant Adapter as antenna adapter
  participant RF as RF / antenna control

  Tick->>Sched: build channel-specific candidates
  Sched->>Helper: resolve BWP, TDA, HARQ, RB/CCE/PUCCH
  Helper-->>Sched: normalized scheduling request
  Sched->>Policy: ask preferred beam for request
  Policy->>Alloc: reserve or update beam resource
  alt beam resource available
    Alloc-->>Sched: reservation granted
    Sched-->>Tick: emit FAPI PDUs
  else temporary resource contention
    Alloc-->>Sched: no beam for this candidate
    Sched-->>Tick: skip or retry later
  else invalid configuration or all candidates failed
    Alloc-->>Sched: configuration failure
    Sched-->>Tick: fatal with explicit reason
  end

  Policy->>Events: publish beam_switch_event_t
  Events->>Adapter: async dispatch outside scheduler hot path
  Adapter->>RF: apply antenna control
  RF-->>Adapter: completion or error
  Note over Sched,Helper: Channel schedulers own channel rules. shared helpers own cross-channel BWP/TDA/resource normalization.
```

#### 責務侵害として見える点

- SIB1 scheduler が Type0 CSS/BWP/RIV 補正を局所的に抱えすぎている。共通の PDCCH/PDSCH BWP 解決 helper に寄せるのが望ましい。
- RA scheduler が beam 決定、BWP 構成、Msg2/Msg4 resource selection を強く結合している。RA context に同期 SSB/beam を保存し、BWP/PDCCH helper がそれを読む構成が望ましい。
- UCI/SR scheduler が beam 不足を fatal にすると、PUCCH scheduling の資源競合判定を越えて gNB 生存性を侵害する。
- beam switch 処理が RRC reconfiguration やアンテナ制御を直接実行する設計は危険である。scheduler の責務は decision/event 発行までに留め、重い I/O は非同期 adapter 側へ逃がすべきである。
