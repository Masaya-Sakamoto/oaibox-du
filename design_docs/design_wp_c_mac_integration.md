# WP-C: MACスケジューラ接続 設計書

## 概要

WP-Bで定義するアンテナ制御インターフェース (`antenna_control_if_t`) を
MACスケジューラの `beam_switching_procedure()` にフックし、
ビーム切替の決定を外部に通知可能にする。

## 背景：ビーム切替のデータフロー

```
CSI Report (UE → gNB)
  ↓
beam_selection_procedures()   ← SSB RSRP から最良ビームを判定
  ↓
beam_switching_procedure()    ← UE_beam_index を更新  ★ここにフック追加
  ↓
[WP-B] on_beam_switch()      ← 外部通知（テスト/アンテナ/ログ）
```

## 改修対象ファイル

### 1. `gNB_scheduler_primitives.c` — `beam_switching_procedure()`

**現状**（3611行付近）：

```c
static void beam_switching_procedure(NR_UE_info_t *UE, int new_beam_index)
{
  LOG_I(NR_MAC, "[UE %x] Switching to beam with ID %d (from %d)\n",
        UE->rnti, new_beam_index, UE->UE_beam_index);
  UE->UE_beam_index = new_beam_index;
}
```

**改修後**：

```c
static void beam_switching_procedure(gNB_MAC_INST *mac,
                                      NR_UE_info_t *UE,
                                      int new_beam_index,
                                      int frame,
                                      int slot)
{
  int old_beam = UE->UE_beam_index;
  LOG_I(NR_MAC, "[UE %x] Switching to beam with ID %d (from %d)\n",
        UE->rnti, new_beam_index, old_beam);
  UE->UE_beam_index = new_beam_index;

  /* WP-C: アンテナ制御コールバック呼出し */
  if (mac->antenna_ctrl && mac->antenna_ctrl->on_beam_switch) {
    beam_switch_event_t event = {
      .rnti = UE->rnti,
      .old_beam_index = (int16_t)old_beam,
      .new_beam_index = (int16_t)new_beam_index,
      .frame = frame,
      .slot = slot,
    };
    mac->antenna_ctrl->on_beam_switch(&event);
  }
}
```

### 注意：関数シグネチャの変更

`beam_switching_procedure()` のシグネチャが変わるため、
呼び出し元もすべて更新する必要がある。

現在の呼び出し元は以下の2箇所：

#### 呼び出し元 1: `nr_mac_update_timers()` (3671行付近)

```c
// 変更前:
beam_switching_procedure(UE, sched_ctrl->UE_mac_ce_ctrl.tci_state_ind.tciStateId);

// 変更後:
beam_switching_procedure(mac, UE,
                          sched_ctrl->UE_mac_ce_ctrl.tci_state_ind.tciStateId,
                          frame, slot);
```

#### 呼び出し元 2: `beam_selection_procedures()` (3831行付近)

```c
// 変更前:
beam_switching_procedure(UE, new_bf_index);

// 変更後（frame/slot を引数に追加が必要 — 下記参照）:
beam_switching_procedure(mac, UE, new_bf_index, frame, slot);
```

### 2. `gNB_scheduler_primitives.c` — `beam_selection_procedures()`

`beam_selection_procedures()` にも `frame` と `slot` を渡す必要がある。
現状は以下のシグネチャ：

```c
void beam_selection_procedures(gNB_MAC_INST *mac, NR_UE_info_t *UE);
```

改修後：

```c
void beam_selection_procedures(gNB_MAC_INST *mac, NR_UE_info_t *UE,
                                int frame, int slot);
```

この関数の呼出元も合わせて更新する。呼出元は `gNB_dlsch_ulsch_scheduler()` 等。

### 3. `mac_proto.h`

プロトタイプ更新：

```c
void beam_selection_procedures(gNB_MAC_INST *mac, NR_UE_info_t *UE,
                                int frame, int slot);
```

## 既存関数は変更不要

以下の関数はそのまま：
- `beam_allocation_procedure()` — ビームリソース予約ロジック
- `reset_beam_status()` — ビームリソース解放
- `fill_beam_index_list()` — SSB-ビームマッピング
- `get_beam_from_ssbidx()` — SSBインデックスからビームID変換

## 検証手順

以下のテストがPASSすることを確認：

```bash
cd openair2/LAYER2/NR_MAC_gNB/tests

# ビーム選択ロジック + コールバック呼出し検証
gcc -o test_beam_selection test_beam_selection.c -Wall -Wextra
./test_beam_selection

# アンテナ制御IF単体テスト
gcc -o test_antenna_control test_antenna_control.c -Wall -Wextra
./test_antenna_control
```

## 依存関係

- **WP-B** の `beam_antenna_control.h/c` が先に必要
- WP-D には依存しない

## 推定工数

**1日**
