#include "screenshot.h"
#include "esp_log.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static const char *TAG = "screenshot";

// Stream a snapshot row-by-row to a FILE* without extra buffering
static bool write_snapshot_rows(FILE *fp, const uint8_t *base, uint32_t stride, uint32_t width, uint32_t height,
                                uint32_t bytes_per_pixel, bool bottom_up) {
    if (!fp || !base || width == 0 || height == 0 || bytes_per_pixel == 0)
        return false;

    // Iterate either bottom->top or top->bottom
    if (bottom_up) {
        for (int32_t y = (int32_t)height - 1; y >= 0; --y) {
            const uint8_t *row = base + (size_t)y * stride;
            size_t row_bytes   = (size_t)width * bytes_per_pixel;
            if (fwrite(row, 1, row_bytes, fp) != row_bytes)
                return false;
        }
    } else {
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t *row = base + (size_t)y * stride;
            size_t row_bytes   = (size_t)width * bytes_per_pixel;
            if (fwrite(row, 1, row_bytes, fp) != row_bytes)
                return false;
        }
    }
    return true;
}

// Map LVGL color formats we’re likely to use to bytes-per-pixel
static inline uint32_t bpp_for_cf(lv_color_format_t cf) {
    switch (cf) {
        case LV_COLOR_FORMAT_RGB565: return 2;
        case LV_COLOR_FORMAT_RGB888: return 3;
        case LV_COLOR_FORMAT_ARGB8888: return 4;
        case LV_COLOR_FORMAT_XRGB8888: return 4;
        default: return 0;
    }
}

bool screenshot_capture_raw(const char *path, lv_obj_t *root, lv_color_format_t cf, bool bottom_up, screenshot_info_t *out_meta) {
    if (!path || !root) {
        ESP_LOGE(TAG, "Invalid args (path/root)");
        return false;
    }

    uint32_t bpp = bpp_for_cf(cf);
    if (bpp == 0) {
        ESP_LOGE(TAG, "Unsupported color format %d for raw dump", (int)cf);
        return false;
    }

    // Take snapshot
    lv_draw_buf_t *snap = lv_snapshot_take(root, cf);
    if (!snap) {
        ESP_LOGE(TAG, "lv_snapshot_take() failed");
        return false;
    }

    const uint32_t w      = snap->header.w;
    const uint32_t h      = snap->header.h;
    const uint32_t stride = snap->header.stride;
    const uint8_t *data   = (const uint8_t *)snap->data;

    if (w == 0 || h == 0 || !data) {
        ESP_LOGE(TAG, "Invalid snapshot (w=%u h=%u data=%p)", w, h, (void *)data);
        lv_draw_buf_destroy(snap);
        return false;
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        lv_draw_buf_destroy(snap);
        return false;
    }

    // Stream rows directly from the snapshot buffer => no large malloc needed
    bool ok = write_snapshot_rows(fp, data, stride, w, h, bpp, bottom_up);

    // Clean up
    fclose(fp);
    lv_draw_buf_destroy(snap);

    if (!ok) {
        ESP_LOGE(TAG, "Failed writing rows to %s", path);
        unlink(path); // remove partial file
        return false;
    }

    if (out_meta) {
        out_meta->width     = w;
        out_meta->height    = h;
        out_meta->stride    = stride;
        out_meta->cf        = cf;
        out_meta->bottom_up = bottom_up;
    }

    ESP_LOGI(TAG, "Snapshot saved: %s (w=%u h=%u cf=%d, %s)", path, w, h, (int)cf, bottom_up ? "bottom-up" : "top-down");
    return true;
}

// ---------------------------------------------------------------------------------------------
// File utilities
// ---------------------------------------------------------------------------------------------

int screenshot_get_size(const char *path) {
    struct stat st;
    if (!path || stat(path, &st) != 0)
        return -1;
    return (int)st.st_size;
}

int screenshot_read(const char *path, uint8_t *buffer, size_t buffer_size) {
    if (!path || !buffer || buffer_size == 0)
        return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;
    size_t n = fread(buffer, 1, buffer_size, fp);
    fclose(fp);
    return (int)n;
}

bool screenshot_delete(const char *path) {
    if (!path)
        return false;
    if (unlink(path) == 0) {
        ESP_LOGI(TAG, "Deleted %s", path);
        return true;
    }
    ESP_LOGE(TAG, "Delete failed for %s", path);
    return false;
}

int screenshot_list(const char *dir_path, const char *filter_ext, char (*filenames)[64], int max_count) {
    if (!dir_path || !filenames || max_count <= 0)
        return 0;

    DIR *dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGE(TAG, "opendir(%s) failed", dir_path);
        return 0;
    }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && count < max_count) {
        const char *name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        if (filter_ext && !strstr(name, filter_ext))
            continue;

        strncpy(filenames[count], name, 63);
        filenames[count][63] = '\0';
        ++count;
    }

    closedir(dir);
    return count;
}

static const char *cf_to_string(lv_color_format_t cf) {
    switch (cf) {
        case LV_COLOR_FORMAT_RGB565: return "RGB565";
        case LV_COLOR_FORMAT_RGB888: return "RGB888";
        case LV_COLOR_FORMAT_ARGB8888: return "ARGB8888";
        case LV_COLOR_FORMAT_XRGB8888: return "XRGB8888";
        default: return "UNKNOWN";
    }
}

bool screenshot_write_meta(const char *path, const screenshot_info_t *meta) {
    if (!path || !meta)
        return false;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return false;

    cJSON_AddNumberToObject(root, "width", (double)meta->width);
    cJSON_AddNumberToObject(root, "height", (double)meta->height);
    cJSON_AddNumberToObject(root, "stride", (double)meta->stride);
    // cJSON_AddNumberToObject(root, "cf", (double)meta->cf);
    cJSON_AddStringToObject(root, "cf", cf_to_string(meta->cf));
    cJSON_AddBoolToObject(root, "bottom_up", meta->bottom_up);

    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        cJSON_Delete(root);
        return false;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        cJSON_free(json_str);
        cJSON_Delete(root);
        return false;
    }

    fputs(json_str, fp);
    fclose(fp);

    cJSON_free(json_str);
    cJSON_Delete(root);
    return true;
}

static bool parse_cf_string(const char *s, lv_color_format_t *out) {
    if (!s || !out)
        return false;
    if (!strcmp(s, "RGB565"))
        *out = LV_COLOR_FORMAT_RGB565;
    else if (!strcmp(s, "RGB888"))
        *out = LV_COLOR_FORMAT_RGB888;
    else if (!strcmp(s, "ARGB8888"))
        *out = LV_COLOR_FORMAT_ARGB8888;
    else if (!strcmp(s, "XRGB8888"))
        *out = LV_COLOR_FORMAT_XRGB8888;
    else
        return false;
    return true;
}

bool screenshot_read_meta(const char *path, screenshot_info_t *out_meta) {
    if (!path || !out_meta)
        return false;

    // Load entire file into memory
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return false;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len <= 0) {
        fclose(fp);
        return false;
    }
    fseek(fp, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return false;
    }

    size_t n = fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root)
        return false;

    // Fetch fields with type checks
    cJSON *j_w  = cJSON_GetObjectItemCaseSensitive(root, "width");
    cJSON *j_h  = cJSON_GetObjectItemCaseSensitive(root, "height");
    cJSON *j_st = cJSON_GetObjectItemCaseSensitive(root, "stride");
    cJSON *j_cf = cJSON_GetObjectItemCaseSensitive(root, "cf");
    cJSON *j_bu = cJSON_GetObjectItemCaseSensitive(root, "bottom_up");

    bool ok = cJSON_IsNumber(j_w) && cJSON_IsNumber(j_h) && cJSON_IsNumber(j_st) &&
              (cJSON_IsNumber(j_cf) || cJSON_IsString(j_cf)) && (cJSON_IsBool(j_bu) || cJSON_IsNumber(j_bu));

    if (!ok) {
        cJSON_Delete(root);
        return false;
    }

    out_meta->width  = (uint32_t)j_w->valuedouble;
    out_meta->height = (uint32_t)j_h->valuedouble;
    out_meta->stride = (uint32_t)j_st->valuedouble;

    // cf can be number or string
    if (cJSON_IsNumber(j_cf)) {
        out_meta->cf = (lv_color_format_t)((int)j_cf->valuedouble);
    } else {
        lv_color_format_t cf_tmp;
        if (!parse_cf_string(j_cf->valuestring, &cf_tmp)) {
            cJSON_Delete(root);
            return false;
        }
        out_meta->cf = cf_tmp;
    }

    // bottom_up accept true/false
    if (cJSON_IsBool(j_bu)) {
        out_meta->bottom_up = cJSON_IsTrue(j_bu);
    } else {
        out_meta->bottom_up = ((int)j_bu->valuedouble) != 0;
    }

    cJSON_Delete(root);
    return true;
}