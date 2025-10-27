#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

typedef struct {
    uint32_t width;       // pixels
    uint32_t height;      // pixels
    uint32_t stride;      // bytes per row in the snapshot buffer
    lv_color_format_t cf; // LV_COLOR_FORMAT_*
    bool bottom_up;       // how it was written to file
} screenshot_info_t;

/**
 * @brief Capture a snapshot of any LVGL object subtree (screen, container, etc.)
 *
 * Writes raw pixel bytes exactly as emitted per row. No image header is added—just raw rows concatenated.
 *
 * @param path       Absolute path in your filesystem (e.g., "/spiffs/foo.raw")
 * @param root       Root object to snapshot (use lv_screen_active() for whole screen)
 * @param cf         Desired LVGL color format (e.g., LV_COLOR_FORMAT_RGB565)
 * @param bottom_up  If true, write rows from bottom to top (handy for BMP-style consumers)
 * @param out_meta   Optional; if non-NULL, filled with w/h/stride/cf/bottom_up
 * @return true on success
 */
bool screenshot_capture_raw(const char *path, lv_obj_t *root, lv_color_format_t cf, bool bottom_up, screenshot_info_t *out_meta);

/**
 * @brief Convenience wrapper: capture active screen in RGB565, top-down
 */
static inline bool screenshot_capture_screen_raw(const char *path) {
    return screenshot_capture_raw(path, lv_screen_active(), LV_COLOR_FORMAT_RGB565, /*bottom_up=*/false, NULL);
}

/**
 * @brief Get the size of a screenshot file
 *
 * @param path The path to the screenshot file
 * @return Size in bytes, or -1 if file doesn't exist
 */
int screenshot_get_size(const char *path);

/**
 * @brief Read screenshot data into buffer
 *
 * @param path The path to the screenshot file
 * @param buffer Buffer to read data into
 * @param buffer_size Size of the buffer
 * @return Number of bytes read, or -1 on error
 */
int screenshot_read(const char *path, uint8_t *buffer, size_t buffer_size);

/**
 * @brief Delete a screenshot file
 *
 * @param path The path to the screenshot file
 * @return true if deleted successfully, false otherwise
 */
bool screenshot_delete(const char *path);

/**
 * @brief List files in a directory (simple helper)
 *
 * @param dir_path   e.g., "/spiffs"
 * @param filter_ext If non-NULL, only include names containing this substring (e.g., ".raw")
 * @param filenames  caller-allocated array [max_count][64]
 * @param max_count  max entries to write
 * @return number of files listed
 */
int screenshot_list(const char *dir_path, const char *filter_ext, char (*filenames)[64], int max_count);

/**
 * @brief Write metadata JSON to a file
 *
 * @param path The path to the screenshot file
 * @param meta The metadata information to write
 * @return true if successful, false otherwise
 */
bool screenshot_write_metadata(const char *path, const screenshot_info_t *meta);

/**
 * @brief Read metadata JSON from a file
 *
 * @param path The path to the screenshot file
 * @param meta The metadata information to read
 * @return true if successful, false otherwise
 */
bool screenshot_read_metadata(const char *path, screenshot_info_t *meta);

#ifdef __cplusplus
}
#endif
