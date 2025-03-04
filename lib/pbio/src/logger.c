// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2020 The Pybricks Authors

#include <pbio/config.h>

#if PBIO_CONFIG_LOGGER

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include <pbdrv/clock.h>
#include <pbio/config.h>
#include <pbio/error.h>
#include <pbio/logger.h>

/**
 * Starts logging in the background.
 * @param [in]  log     pointer to log
 * @param [in]  buf     array large enough to hold @p len rows of data
 * @param [in]  len     maximum number of rows that can be logged
 * @param [in]  div     clock divider to slow down sampling period
 */
void pbio_logger_start(pbio_log_t *log, int32_t *buf, uint32_t len, int32_t div) {
    // (re-)initialize logger status for this servo
    log->sampled = 0;
    log->skipped = 0;
    log->data = buf;
    log->len = len;
    log->sample_div = div;
    log->start = pbdrv_clock_get_ms();
    log->active = true;
}

int32_t pbio_logger_rows(pbio_log_t *log) {
    return log->sampled;
}

int32_t pbio_logger_cols(pbio_log_t *log) {
    return log->num_values;
}

void pbio_logger_stop(pbio_log_t *log) {
    // Release the logger for re-use
    log->active = false;
}

void pbio_logger_update(pbio_log_t *log, int32_t *buf) {

    // Log nothing if logger is inactive
    if (!log->active) {
        return;
    }

    // Skip logging if we are not yet at a multiple of sample_div
    if (++log->skipped != log->sample_div) {
        return;
    }
    log->skipped = 0;

    // Stop if log is full.
    if (log->sampled > log->len) {
        log->active = false;
        return;
    }

    // Stop successfully when done
    if (log->sampled == log->len) {
        log->active = false;
        return;
    }

    // Write time of logging
    log->data[log->sampled * log->num_values] = pbdrv_clock_get_ms() - log->start;

    // Write the data
    for (uint8_t i = NUM_DEFAULT_LOG_VALUES; i < log->num_values; i++) {
        log->data[log->sampled * log->num_values + i] = buf[i - NUM_DEFAULT_LOG_VALUES];
    }

    // Increment sample counter
    log->sampled++;

    return;
}

pbio_error_t pbio_logger_read(pbio_log_t *log, int32_t sindex, int32_t *buf) {

    // Validate index value
    if (sindex < -1) {
        return PBIO_ERROR_INVALID_ARG;
    }

    // Get index or latest sample if requested index is -1
    uint32_t index = sindex < 0 ? log->sampled - 1 : (uint32_t)sindex;

    // Ensure index is within bounds
    if (index >= log->sampled) {
        return PBIO_ERROR_INVALID_ARG;
    }

    // Read the data
    for (uint8_t i = 0; i < log->num_values; i++) {
        buf[i] = log->data[index * log->num_values + i];
    }

    return PBIO_SUCCESS;
}

#endif // PBIO_CONFIG_LOGGER
