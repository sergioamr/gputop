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


#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>

#include "gputop-util.h"

#include <gputop-string.h>
#include <gputop-oa-counters.h>
#include <gputop-list.h>

#include "gputop-accumulator.h"

/* Cache containing the PIDs using the least significant bits of the pid
 * to accelerate the search.
 */
#define MAX_CACHE_SIZE (1<<16)
struct gputop_accumulator *acc_cache[MAX_CACHE_SIZE];

gputop_list_t accumulator_list;
bool accumulator_init = false;

/* Finds the accumulator in a cache by pid and opengl context id
 * If context id is 0 it will return the first accumulator with that id
 */
struct gputop_accumulator *
gputop_accumulator_fast_find(uint32_t pid, uint32_t ctx)
{
    uint32_t idx = pid & (MAX_CACHE_SIZE-1);
    struct gputop_accumulator *acc = acc_cache[idx];
    if (!acc)
        return NULL;

    do {
        if (acc->pid == pid) {
            if (ctx == 0 || ctx == acc->ctx_id) {
                return acc;
            }
        }
        acc++;
    } while (!acc);

    return NULL;
}

/* Finds the accumulator in the list by pid and opengl context id
 * If context id is 0 it will return the first accumulator with that id
 */
struct gputop_accumulator *
gputop_accumulator_find(uint32_t pid, uint32_t ctx)
{
    struct gputop_accumulator *acc = NULL;
    gputop_list_for_each(acc, &accumulator_list, link) {
        if (acc->pid == pid) {
            if (ctx == 0 || ctx == acc->ctx_id) {
                 return acc;
             }
        }
    }
    return NULL;
}

/* Tries to add an accumulator, in case it exists it will
 * return the last one
 */
struct gputop_accumulator *
gputop_add_accumulator(uint32_t pid, uint32_t ctx)
{
    struct gputop_accumulator *acc = gputop_accumulator_fast_find(pid, ctx);
    if (acc != NULL) {
        return acc;
    }

    acc = xmalloc0(sizeof(*acc));
    acc->pid = pid;
    acc->ctx_id = ctx;
    gputop_accumulator_insert(acc);
    return acc;
}

//TODO(@sergioamr) Add emscripten flag
//static void EMSCRIPTEN_KEEPALIVE
void
gputop_accumulator_init()
{
    if (accumulator_init)
        return;

    accumulator_init = true;
    memset(acc_cache, 0, sizeof(acc_cache));
    gputop_list_init(&accumulator_list);
}

struct gputop_accumulator *
gputop_accumulator_insert(struct gputop_accumulator *acc)
{
    uint32_t idx = acc->pid & (MAX_CACHE_SIZE -1);
    gputop_list_insert(accumulator_list.prev, &acc->link);

    while (acc_cache[idx]!=NULL)
        idx++;

    acc_cache[idx]=acc;
    return acc;
}

bool
gputop_accumulator_delete(struct gputop_accumulator *acc)
{
    return false;
}

int
unit_tests() {
    gputop_accumulator_init();
    return 0;
}
