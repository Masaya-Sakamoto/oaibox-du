# WP-D: RFsimチャネル外部制御 設計書

## 概要

RFsimulatorに `setpathloss` telnetコマンドを追加し、
テストスクリプトから動的にチャネルのpath_lossを変更可能にする。
これにより、統合テスト (`run_beam_switch_test.sh`) がtelnet経由で
チャネル変動シナリオを注入し、ビーム切替をトリガーできる。

## 背景：既存のRFsimチャネル制御基盤

RFsimには既に以下のチャネル制御機能が存在する：

| 機能 | telnetコマンド | 対象パラメータ |
|---|---|---|
| ビームゲインマトリクス | `setbeam` | `beam_gains[][]` |
| ビームID設定 | `setbeamids` | `beam_state_t` のキュー |
| 距離変更 | `setdistance` | `channel_offset` |
| チャネルモデル切替 | `setmodel` | `channel_desc_t→modelname` |

**不足している機能**: `channel_desc_t→path_loss_dB` を直接変更する手段がない。
`setdistance` は伝搬遅延（`channel_offset`）を変更するが、path_lossは変化しない。

## path_lossの適用パス

```
channel_desc_t→path_loss_dB
  ↓
rxAddInput() [apply_channelmod.c:246]
  ↓
  gain_factor = pow(10.0, path_loss_dB / 20.0)
  ↓
  受信サンプルに乗算
```

`path_loss_dB` を変更すれば、受信信号のパワーが即座に変化し、
UE側のRSRP測定値に影響を与えることで、gNBのビーム切替判定に反映される。

## 改修対象

### `radio/rfsimulator/simulator.cpp`

#### 新規関数: `rfsimu_setpathloss_cmd()`

以下の関数をtelnetコマンドハンドラとして追加する：

```cpp
static int rfsimu_setpathloss_cmd(char *buff, int debug, telnet_printfunc_t prnt, void *arg)
{
  char *modelname = NULL;
  double pathloss_dB;
  int s = sscanf(buff, "%m[^ ] %lf\n", &modelname, &pathloss_dB);
  if (s != 2) {
    prnt("%s: usage: setpathloss <model_name> <pathloss_dB>\n", __func__);
    prnt("  example: setpathloss rfsimu_channel_ue0 -20.0\n");
    free(modelname);
    return CMDSTATUS_VARNOTFOUND;
  }

  rfsimulator_state_t *t = (rfsimulator_state_t *)arg;
  bool found = false;

  for (int i = 0; i < MAX_FD_RFSIMU; i++) {
    buffer_t *b = &t->buf[i];
    if (b->conn_sock > 0 && b->channel_model
        && b->channel_model->model_name
        && strcmp(b->channel_model->model_name, modelname) == 0) {
      double old_pl = b->channel_model->path_loss_dB;
      b->channel_model->path_loss_dB = pathloss_dB;
      prnt("path_loss_dB: %.1f -> %.1f for channel '%s'\n",
           old_pl, pathloss_dB, modelname);
      found = true;
    }
  }

  if (!found) {
    prnt("Channel '%s' not found. Active channels:\n", modelname);
    for (int i = 0; i < MAX_FD_RFSIMU; i++) {
      buffer_t *b = &t->buf[i];
      if (b->conn_sock > 0 && b->channel_model && b->channel_model->model_name)
        prnt("  - %s (path_loss_dB=%.1f)\n",
             b->channel_model->model_name, b->channel_model->path_loss_dB);
    }
    free(modelname);
    return CMDSTATUS_VARNOTFOUND;
  }

  free(modelname);
  return CMDSTATUS_FOUND;
}
```

#### `rfsimu_cmdarray[]` への登録

既存のコマンド配列（`rfsimu_cmdarray[]`）に以下のエントリを追加：

```cpp
// rfsimu_cmdarray[] の既存エントリの後に追加:
{"setpathloss",
 "Set channel path_loss_dB for a specific channel model.\n"
 "Usage: setpathloss <model_name> <pathloss_dB>\n"
 "Example: setpathloss rfsimu_channel_ue0 -20.0\n",
 TELNETSRV_CMDFUNC, {.cmdfunc = rfsimu_setpathloss_cmd}},
```

## コマンド配列の場所

`rfsimu_cmdarray[]` は `simulator.cpp` の下部にある。
既存のコマンド一覧は以下の通り：

| コマンド | 関数 |
|---|---|
| `setbeam` | `rfsimu_setbeam_cmd` |
| `setbeamids` | `rfsimu_setbeamids_cmd` |
| `setdistance` | `rfsimu_setdist_cmd` |
| `setmodel` | `rfsimu_setmodel_cmd` |
| `vtime` | `rfsimu_vtime_cmd` |

`setpathloss` をこのリストの末尾に追加する。

## テストスクリプトからの使用例

```bash
# telnet で接続して path_loss_dB を変更
(echo "rfsimu setpathloss rfsimu_channel_ue0 -20.0"; sleep 1; echo "exit") \
  | telnet localhost 9090
```

### シナリオCSV（オプション、テストスクリプトから利用）

```csv
# time_sec, model_name, pathloss_dB
0.0,  rfsimu_channel_ue0, -3.0
2.0,  rfsimu_channel_ue0, -10.0
4.0,  rfsimu_channel_ue0, -20.0
6.0,  rfsimu_channel_ue0, -3.0
```

テストスクリプト側で `sleep` + `telnet` を使って注入する。
RFsim本体の改変は `setpathloss` コマンドのみで十分。

## 検証手順

### 1. ビルド

```bash
cd cmake_targets
./build_oai -w SIMU --ninja --gNB
```

### 2. 手動テスト

```bash
# Terminal 1: gNB 起動
RFSIMULATOR=server ./nr-softmodem -O <conf> --sa --rfsim --telnetsrv

# Terminal 2: テスト
telnet localhost 9090
> rfsimu setpathloss rfsimu_channel_ue0 -20.0
# 出力: path_loss_dB: 0.0 -> -20.0 for channel 'rfsimu_channel_ue0'
```

### 3. 統合テスト

```bash
cd openair2/LAYER2/NR_MAC_gNB/tests
./run_beam_switch_test.sh
```

## 依存関係

- 他のWPに依存しない（独立して並行開発可）

## 推定工数

**1日**
