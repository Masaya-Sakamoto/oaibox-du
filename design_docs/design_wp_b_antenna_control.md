# WP-B: アンテナ制御インターフェース設計書

## 概要

MAC層のビームスケジューラ（`beam_switching_procedure`）がビーム切替を決定した際に、
その情報を外部（テスト検証・実アレイアンテナ制御・ログ統計）に通知するための
コールバック・ベースのインターフェースを新規作成する。

## 設計意図

OAI gNBのMAC層は、CSI ReportのRSRPに基づき最適ビームを選択し
`UE->UE_beam_index`を更新する。しかし、現状この情報はMAC層内部に閉じており、
実際のアレイアンテナ（位相制御器・BFN）やテストフレームワークから
ビーム切替イベントを観測・フックする手段がない。

本インターフェースは以下の3つの使用場面を想定している：

1. **テスト検証**（最優先）  
   テストコードがモック関数を `on_beam_switch` に設定し、ビーム切替の発生と
   パラメータの正しさを検証する。

2. **実アレイアンテナ制御**（将来）  
   位相制御器やRFICの設定変更を、MACのビーム決定に連動させる。
   `event->new_beam_index` に応じて位相シフタ設定をキューに積む。

3. **ログ・統計**（デフォルト実装）  
   ビーム切替頻度のモニタリング用。デフォルトではログ出力のみ。

## 呼び出しタイミング

`beam_switching_procedure()` 内で `UE->UE_beam_index` が  
`old != new` で更新された直後の **1回のみ** 呼ばれる。

SSB送信時には呼ばれない（SSBは全ビームで送信するため、切替ではない）。

## 新規ファイル

### `openair2/LAYER2/NR_MAC_gNB/beam_antenna_control.h`

```c
#ifndef __BEAM_ANTENNA_CONTROL_H__
#define __BEAM_ANTENNA_CONTROL_H__

#include <stdint.h>

/* ビーム切替イベント構造体 */
typedef struct {
    uint16_t rnti;            // 対象UEのRNTI
    int16_t  old_beam_index;  // 切替前のビームID
    int16_t  new_beam_index;  // 切替後のビームID
    int      frame;           // 切替発生フレーム番号
    int      slot;            // 切替発生スロット番号
} beam_switch_event_t;

/* コールバック関数型
 *
 * beam_switching_procedure() の内部で、UE_beam_index が変更された直後に呼ばれる。
 * スケジューラスレッド内で同期的に呼ばれるため、重い処理は避けること。
 * 非同期処理が必要な場合はキューに積んで別スレッドで処理を推奨。
 */
typedef void (*beam_switch_notify_fn)(const beam_switch_event_t *event);

/* アンテナ制御インターフェース */
typedef struct {
    beam_switch_notify_fn on_beam_switch;  // ビーム切替通知コールバック
    void *priv;                            // 実装固有のプライベートデータ
} antenna_control_if_t;

/* デフォルト実装を生成（ログ出力のみ）*/
antenna_control_if_t *create_default_antenna_ctrl(void);

/* インターフェースの破棄 */
void destroy_antenna_ctrl(antenna_control_if_t *ctrl);

#endif /* __BEAM_ANTENNA_CONTROL_H__ */
```

### `openair2/LAYER2/NR_MAC_gNB/beam_antenna_control.c`

```c
#include "beam_antenna_control.h"
#include "common/utils/LOG/log.h"
#include <stdlib.h>

static void default_beam_switch_notify(const beam_switch_event_t *event)
{
    LOG_I(NR_MAC,
          "[AntennaCtrl] Beam switch: RNTI=0x%04x beam %d -> %d at (%d.%d)\n",
          event->rnti,
          event->old_beam_index,
          event->new_beam_index,
          event->frame,
          event->slot);
}

antenna_control_if_t *create_default_antenna_ctrl(void)
{
    antenna_control_if_t *ctrl = calloc(1, sizeof(*ctrl));
    if (!ctrl)
        return NULL;
    ctrl->on_beam_switch = default_beam_switch_notify;
    ctrl->priv = NULL;
    return ctrl;
}

void destroy_antenna_ctrl(antenna_control_if_t *ctrl)
{
    free(ctrl);
}
```

## MAC構造体への統合

### `nr_mac_gNB.h` の変更

`gNB_MAC_INST` 構造体に以下のフィールドを追加する（935行付近）：

```c
#include "beam_antenna_control.h"

typedef struct gNB_MAC_INST_s {
    // ... 既存フィールド ...
    antenna_control_if_t *antenna_ctrl;   /* ビーム切替通知インターフェース */
    // ... 既存フィールド ...
} gNB_MAC_INST;
```

### `mac_proto.h` への追加

```c
/* beam_antenna_control.h */
antenna_control_if_t *create_default_antenna_ctrl(void);
void destroy_antenna_ctrl(antenna_control_if_t *ctrl);
```

### MAC初期化での呼出し

`config.c` もしくは MAC初期化関数で：

```c
mac->antenna_ctrl = create_default_antenna_ctrl();
```

## 検証手順

以下のテストがPASSすることを確認：

```bash
cd openair2/LAYER2/NR_MAC_gNB/tests
gcc -o test_antenna_control test_antenna_control.c -Wall -Wextra
./test_antenna_control
```

## 推定工数

**0.5日**
