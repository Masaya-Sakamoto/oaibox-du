#ifndef PTR_BLOCKING_QUEUE_H
#define PTR_BLOCKING_QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    void** buffer;        // void* ポインタの配列
    size_t capacity;      // キューの最大要素数
    
    size_t head;          // 書き込み位置
    size_t tail;          // 読み込み位置
    size_t count;         // 現在の要素数

    bool is_shutdown;     // 終了フラグ

    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;
} ptr_blocking_queue_t;

// キューの生成（ポインタを格納するバッファを確保）
static inline ptr_blocking_queue_t* pbq_create(size_t capacity) {
    ptr_blocking_queue_t* q = (ptr_blocking_queue_t*)malloc(sizeof(ptr_blocking_queue_t));
    if (!q) return NULL;

    // void* 型の配列を capacity 分確保
    q->buffer = (void**)malloc(capacity * sizeof(void*));
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->is_shutdown = false;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond_not_empty, NULL);
    pthread_cond_init(&q->cond_not_full, NULL);

    return q;
}

// キューの破棄（キュー自体のメモリ解放）
// ※注意: キューの中に残っているポインタ先のメモリは解放しません。
// 必要に応じて破棄前に全てpopしてfreeする必要があります。
static inline void pbq_destroy(ptr_blocking_queue_t* q) {
    if (!q) return;
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond_not_empty);
    pthread_cond_destroy(&q->cond_not_full);
    free(q->buffer);
    free(q);
}

// シャットダウン（待機中のスレッドをすべて強制起床させる）
static inline void pbq_shutdown(ptr_blocking_queue_t* q) {
    pthread_mutex_lock(&q->mutex);
    q->is_shutdown = true;
    pthread_cond_broadcast(&q->cond_not_empty);
    pthread_cond_broadcast(&q->cond_not_full);
    pthread_mutex_unlock(&q->mutex);
}

// エンキュー（ポインタをキューに格納）
// 戻り値: 成功時は true、シャットダウン時は false
static inline bool pbq_push(ptr_blocking_queue_t* q, void* item) {
    pthread_mutex_lock(&q->mutex);

    // 満杯の場合は待機
    while (q->count == q->capacity && !q->is_shutdown) {
        pthread_cond_wait(&q->cond_not_full, &q->mutex);
    }

    if (q->is_shutdown) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    // ポインタを保存
    q->buffer[q->head] = item;
    q->head = (q->head + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->cond_not_empty);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

// デキュー（キューからポインタを取り出す）
// 戻り値: 成功時は true、シャットダウン＆空の場合は false
static inline bool pbq_pop(ptr_blocking_queue_t* q, void** out_item) {
    pthread_mutex_lock(&q->mutex);

    // 空の場合は待機
    while (q->count == 0 && !q->is_shutdown) {
        pthread_cond_wait(&q->cond_not_empty, &q->mutex);
    }

    // シャットダウンされていて、かつ残りのデータが無い場合のみ終了
    if (q->count == 0 && q->is_shutdown) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    // ポインタを取り出して呼び出し元に返す
    *out_item = q->buffer[q->tail];
    q->tail = (q->tail + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->cond_not_full);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

// ノンブロッキングエンキュー（キューが満杯なら即座に false を返す）
// UE_Thread などリアルタイムスレッドから呼ぶ用途向け。
// 戻り値: エンキュー成功時は true、満杯またはシャットダウン時は false
static inline bool pbq_try_push(ptr_blocking_queue_t* q, void* item) {
    pthread_mutex_lock(&q->mutex);

    if (q->is_shutdown || q->count == q->capacity) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    q->buffer[q->head] = item;
    q->head = (q->head + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->cond_not_empty);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

#endif // PTR_BLOCKING_QUEUE_H
