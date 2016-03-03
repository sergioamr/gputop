/*
 * GPU Top
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _GPUTOP_ACCUMULATOR_H_
#define _GPUTOP_ACCUMULATOR_H_

#include <stdbool.h>

#ifndef EMSCRIPTEN
#include <uv.h>
#include <time.h>
#endif

/*
 * Accumulators are in a intrusive linked list
 *
 * The accumulator contains the metric id, pid and ctx_id
 * If ctx_id is zero it will give the first pid found.
 *
 * There is also a cache helper for fast access
 * more compatible with the way this will be stored
 * in javascript.
 */

struct gputop_accumulator {
    int id;

    int metric;
    int pid;
    int ctx_id;

    uint64_t aggregation_period;
    uint32_t sample_flags;

    struct oa_clock_ {
        uint64_t start;
        uint64_t timestamp;
        uint32_t last_raw;
    } oa_clock_;

    uint64_t start_timestamp;
    uint64_t end_timestamp;

    struct gputop_perf_query *oa_query;

    /* Aggregation may happen accross multiple perf data messages
     * so we may need to copy the last report so that aggregation
     * can continue with the next message... */
    uint8_t *continuation_report;

    gputop_list_t link;
};

struct gputop_accumulator *
gputop_accumulator_fast_find(uint32_t pid, uint32_t ctx);

struct gputop_accumulator *
gputop_accumulator_find(uint32_t pid, uint32_t ctx);

/* Finds an accumulator or returns an initialized one */
struct gputop_accumulator *
gputop_add_accumulator(uint32_t pid, uint32_t ctx);

void gputop_accumulator_init();

struct gputop_accumulator *
gputop_accumulator_insert(struct gputop_accumulator *acc);

bool
gputop_accumulator_delete(struct gputop_accumulator *acc);

#endif /* _GPUTOP_ACCUMULATOR_H_ */
