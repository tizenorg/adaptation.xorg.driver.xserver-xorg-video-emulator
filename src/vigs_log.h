/*
 * X.Org X server driver for VIGS
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact :
 * Stanislav Vorobiov <s.vorobiov@samsung.com>
 * Jinhyung Jo <jinhyung.jo@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#ifndef _VIGS_LOG_H
#define _VIGS_LOG_H

typedef enum
{
    vigs_log_level_off = 0,
    vigs_log_level_error = 1,
    vigs_log_level_warn = 2,
    vigs_log_level_info = 3,
    vigs_log_level_debug = 4,
    vigs_log_level_trace = 5
} vigs_log_level;

#define vigs_log_level_max vigs_log_level_trace

void vigs_log_event(vigs_log_level log_level,
                    const char *func,
                    int line,
                    const char *format, ...);

int vigs_log_is_enabled_for_level(vigs_log_level log_level);

#define VIGS_LOG_EVENT(log_level, format, ...) \
    do { \
        if (vigs_log_is_enabled_for_level(vigs_log_level_##log_level)) { \
            vigs_log_event(vigs_log_level_##log_level, __FUNCTION__, __LINE__, format,##__VA_ARGS__); \
        } \
    } while(0)

#define VIGS_LOG_TRACE(format, ...) VIGS_LOG_EVENT(trace, format,##__VA_ARGS__)
#define VIGS_LOG_DEBUG(format, ...) VIGS_LOG_EVENT(debug, format,##__VA_ARGS__)
#define VIGS_LOG_INFO(format, ...) VIGS_LOG_EVENT(info, format,##__VA_ARGS__)
#define VIGS_LOG_WARN(format, ...) VIGS_LOG_EVENT(warn, format,##__VA_ARGS__)
#define VIGS_LOG_ERROR(format, ...) VIGS_LOG_EVENT(error, format,##__VA_ARGS__)

#endif
