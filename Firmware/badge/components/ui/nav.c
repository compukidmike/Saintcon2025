#include "nav.h"
#include "esp_log.h"
#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "nav";

// Navigation constants
#define NAV_CONE_RATIO           0.6
#define NAV_SKIP_ITERATION_LIMIT 64 // Safety limit for skip attempts
#define NAV_ROW_TOLERANCE_PX     16 // Tolerance in pixels to consider an object to be in the same row

struct nav_node_t {
    lv_obj_t *obj;
    lv_obj_t *nbr[NAV_DIR_COUNT];
    struct nav_node_t *next;
};

// ---------------------------------------------------------------------------------------------
// Geometry Helpers
// ---------------------------------------------------------------------------------------------

typedef struct {
    int16_t left, top, right, bottom;
} rect_t;

static inline void get_rect(const lv_obj_t *o, rect_t *r) {
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    r->left   = a.x1;
    r->top    = a.y1;
    r->right  = a.x2;
    r->bottom = a.y2;
}
static inline int16_t center_x(const rect_t *r) {
    return (int16_t)((r->left + r->right) / 2);
}
static inline int16_t center_y(const rect_t *r) {
    return (int16_t)((r->top + r->bottom) / 2);
}

static inline bool axis_overlap(int16_t a1, int16_t a2, int16_t b1, int16_t b2, int16_t tol) {
    // [a1,a2] intersects [b1,b2] with tolerance
    return !(a2 < b1 - tol || b2 < a1 - tol);
}

static bool passes_projection(lv_obj_t *from, lv_obj_t *to, nav_dir_t dir, int16_t tol_px) {
    rect_t A, B;
    get_rect(from, &A);
    get_rect(to, &B);
    switch (dir) {
        case NAV_DIR_RIGHT:
            if (B.left < A.right) {
                return false;
            }
            return axis_overlap(A.top, A.bottom, B.top, B.bottom, tol_px);
        case NAV_DIR_LEFT:
            if (B.right > A.left) {
                return false;
            }
            return axis_overlap(A.top, A.bottom, B.top, B.bottom, tol_px);
        case NAV_DIR_DOWN:
            if (B.top < A.bottom) {
                return false;
            }
            return axis_overlap(A.left, A.right, B.left, B.right, tol_px);
        case NAV_DIR_UP:
            if (B.bottom > A.top) {
                return false;
            }
            return axis_overlap(A.left, A.right, B.left, B.right, tol_px);
        default: return false;
    }
}

static void distances(lv_obj_t *from, lv_obj_t *to, nav_dir_t dir, int32_t *ndd, int32_t *orth, int32_t *manhattan) {
    rect_t A, B;
    get_rect(from, &A);
    get_rect(to, &B);
    int16_t ax = center_x(&A), ay = center_y(&A);
    int16_t bx = center_x(&B), by = center_y(&B);

    switch (dir) {
        case NAV_DIR_RIGHT:
            *ndd  = (int32_t)B.left - (int32_t)A.right;
            *orth = abs(by - ay);
            break;
        case NAV_DIR_LEFT:
            *ndd  = (int32_t)A.left - (int32_t)B.right;
            *orth = abs(by - ay);
            break;
        case NAV_DIR_DOWN:
            *ndd  = (int32_t)B.top - (int32_t)A.bottom;
            *orth = abs(bx - ax);
            break;
        case NAV_DIR_UP:
            *ndd  = (int32_t)A.top - (int32_t)B.bottom;
            *orth = abs(bx - ax);
            break;
        default: *ndd = *orth = 0; break;
    }
    // Manhattan distance between centers (for final tie-break)
    *manhattan = abs(bx - ax) + abs(by - ay);
}

// ---------------------------------------------------------------------------------------------
// Helper Functions
// ---------------------------------------------------------------------------------------------

static nav_node_t *find_node(nav_ctx_t *ctx, const lv_obj_t *o) {
    for (nav_node_t *n = ctx->head; n; n = n->next) {
        if (n->obj == o) {
            return n;
        }
    }
    return NULL;
}

static void obj_center(const lv_obj_t *o, lv_point_t *p) {
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    p->x = (a.x1 + a.x2) / 2;
    p->y = (a.y1 + a.y2) / 2;
}

// Visibility and focusability checks
static bool is_focusable(lv_obj_t *o) {
    if (!o) {
        return false;
    }

    if (lv_obj_has_state(o, LV_STATE_DISABLED)) {
        return false;
    }
    for (lv_obj_t *p = o; p; p = lv_obj_get_parent(p)) {
        if (lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN)) {
            return false;
        }
    }
    if (lv_obj_get_screen(o) != lv_screen_active()) {
        return false;
    }
    return true;
}

static bool is_focus_target(const lv_obj_t *o) {
    if (!o) {
        return false;
    }

    // If LVGL thinks focusing it requires a group, it's probably meaningful
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICK_FOCUSABLE)) {
        return true;
    }

    // Typical actionable widgets
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE)) {
        return true;
    }
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_CHECKABLE)) {
        return true;
    }

    // Exclude pure decorative images/labels
    if (lv_obj_check_type(o, &lv_label_class) || lv_obj_check_type(o, &lv_image_class)) {
        return false;
    }

    return false;
}

// Check if the object is an editable widget (slider, roller, spinbox, textarea, dropdown)
static bool is_editable_widget(const lv_obj_t *o) {
    return lv_obj_check_type(o, &lv_slider_class) || lv_obj_check_type(o, &lv_roller_class) ||
           lv_obj_check_type(o, &lv_spinbox_class) || lv_obj_check_type(o, &lv_textarea_class) ||
           lv_obj_check_type(o, &lv_dropdown_class);
}

// If current object is not focusable (e.g., disabled), try to advance in dir within the group
// until we find a focusable one; otherwise return NULL.
static lv_obj_t *skip_to_focusable_in_group(lv_group_t *g, nav_dir_t dir) {
    lv_obj_t *start = lv_group_get_focused(g);
    lv_obj_t *cur   = start;

    for (int i = 0; i < NAV_SKIP_ITERATION_LIMIT; i++) {
        if (is_focusable(cur)) {
            return cur;
        }

        if (dir == NAV_DIR_UP || dir == NAV_DIR_LEFT) {
            lv_group_focus_prev(g);
        } else {
            lv_group_focus_next(g);
        }

        lv_obj_t *next = lv_group_get_focused(g);
        if (next == cur) {
            break;
        }
        cur = next;
    }
    return NULL;
}

// Find the nearest scrollable ancestor
static lv_obj_t *scroll_root(lv_obj_t *o) {
    lv_obj_t *p = o;
    while (p) {
        if (lv_obj_has_flag(p, LV_OBJ_FLAG_SCROLLABLE)) {
            return p;
        }
        p = lv_obj_get_parent(p);
    }
    return NULL;
}

// Choose best candidate in a given direction based on vector and distance
// static lv_obj_t *pick_in_direction(lv_obj_t *from, nav_dir_t dir, nav_ctx_t *ctx) {
//     if (!from) {
//         return NULL;
//     }

//     lv_point_t pf;
//     obj_center(from, &pf);

//     double best_score = 1e300;
//     lv_obj_t *best    = NULL;

//     for (nav_node_t *n = ctx->head; n; n = n->next) {
//         lv_obj_t *to = n->obj;
//         if (to == from) {
//             continue;
//         }
//         if (!is_focusable(to)) {
//             continue;
//         }
//         if (dir == NAV_DIR_UP || dir == NAV_DIR_DOWN) {
//             if (scroll_root(to) != scroll_root(from)) {
//                 continue;
//             }
//         }

//         lv_point_t pt;
//         obj_center(to, &pt);
//         int dx = pt.x - pf.x;
//         int dy = pt.y - pf.y;

//         // Calculate axis-aligned distances
//         double adx = fabs((double)dx);
//         double ady = fabs((double)dy);

//         switch (dir) {
//             case NAV_DIR_UP:
//                 if (dy >= 0) {
//                     continue;
//                 }
//                 if (adx > ady * (1.0 / NAV_CONE_RATIO)) {
//                     continue; // mostly vertical
//                 }
//                 break;
//             case NAV_DIR_DOWN:
//                 if (dy <= 0) {
//                     continue;
//                 }
//                 if (adx > ady * (1.0 / NAV_CONE_RATIO)) {
//                     continue; // mostly vertical
//                 }
//                 break;
//             case NAV_DIR_LEFT:
//                 if (dx >= 0) {
//                     continue;
//                 }
//                 if (ady > adx * (1.0 / NAV_CONE_RATIO)) {
//                     continue; // mostly horizontal
//                 }
//                 break;
//             case NAV_DIR_RIGHT:
//                 if (dx <= 0) {
//                     continue;
//                 }
//                 if (ady > adx * (1.0 / NAV_CONE_RATIO)) {
//                     continue; // mostly horizontal
//                 }
//                 break;
//             default: break;
//         }

//         // Penalize off-axis angle / prioritize more aligned candidates
//         double dist = sqrt((double)dx * dx + (double)dy * dy);

//         // Weight: distance plus a small penalty for off-axis deviation
//         double penalty = (dir == NAV_DIR_LEFT || dir == NAV_DIR_RIGHT) ? (ady * 0.35) : (adx * 0.35);

//         // Combine distance and penalty into a single score
//         double score = dist + penalty;

//         // Prefer candidates that are more aligned with the navigation direction
//         if (score < best_score) {
//             best_score = score;
//             best       = to;
//         }
//     }

//     return best;
// }

static lane_mem_t *lane_get(nav_ctx_t *ctx, const lv_obj_t *container) {
    for (lane_mem_t *mem = ctx->lanes; mem; mem = mem->next) {
        if (mem->container == container) {
            return mem;
        }
    }
    return NULL;
}

// -------------------------------------------------------------------------------------------------
// Vertical binding tracking
// -------------------------------------------------------------------------------------------------
typedef struct vertical_bind_t {
    lv_obj_t *upper_row;
    lv_obj_t *lower_row;
    bool remember_lane;
    struct vertical_bind_t *next;
} vertical_bind_t;

static vertical_bind_t *vbind_find(nav_ctx_t *ctx, lv_obj_t *upper_row, lv_obj_t *lower_row) {
    for (vertical_bind_t *vb = ctx->vbinds; vb; vb = vb->next) {
        if (vb->upper_row == upper_row && vb->lower_row == lower_row) {
            return vb;
        }
    }
    return NULL;
}

static void vbind_add(nav_ctx_t *ctx, lv_obj_t *upper_row, lv_obj_t *lower_row, bool remember_lane) {
    if (!ctx || !upper_row || !lower_row) {
        return;
    }
    if (vbind_find(ctx, upper_row, lower_row)) {
        return; // already recorded
    }
    vertical_bind_t *vb = calloc(1, sizeof(*vb));
    if (!vb) {
        ESP_LOGE(TAG, "vbind_add: OOM");
        return;
    }
    vb->upper_row     = upper_row;
    vb->lower_row     = lower_row;
    vb->remember_lane = remember_lane;
    vb->next          = ctx->vbinds;
    ctx->vbinds       = vb;
}

// Called when a focus within an upper row changes so we can repoint UP neighbors of the lower row dynamically
static void vbind_refresh_for_upper(nav_ctx_t *ctx, lv_obj_t *upper_row) {
    if (!ctx || !upper_row) {
        return;
    }
    lane_mem_t *lm = lane_get(ctx, upper_row);
    if (!lm) {
        return; // no remembered lane; nothing to do
    }
    for (vertical_bind_t *vb = ctx->vbinds; vb; vb = vb->next) {
        if (vb->upper_row != upper_row) {
            continue;
        }
        uint32_t up_n = lv_obj_get_child_count(vb->upper_row);
        uint32_t lo_n = lv_obj_get_child_count(vb->lower_row);
        if (up_n == 0 || lo_n == 0) {
            continue;
        }
        uint32_t idx     = lm->last_index < up_n ? lm->last_index : (up_n - 1);
        lv_obj_t *target = lv_obj_get_child(vb->upper_row, idx);
        if (!is_focus_target(target)) {
            continue;
        }
        for (uint32_t i = 0; i < lo_n; ++i) {
            lv_obj_t *lo_ch = lv_obj_get_child(vb->lower_row, i);
            if (!is_focus_target(lo_ch)) {
                continue;
            }
            nav_set_neighbor(ctx, lo_ch, NAV_DIR_UP, target);
        }
    }
}

static lane_mem_t *lane_ensure(nav_ctx_t *ctx, lv_obj_t *container) {
    lane_mem_t *mem = lane_get(ctx, container);
    if (mem) {
        return mem;
    }
    mem = calloc(1, sizeof(*mem));
    if (!mem) {
        ESP_LOGE(TAG, "lane_ensure: out of memory");
        return NULL;
    }
    mem->container  = container;
    mem->last_index = 0;
    mem->next       = ctx->lanes;
    ctx->lanes      = mem;
    return mem;
}

static uint32_t child_index_of(const lv_obj_t *parent, const lv_obj_t *child) {
    uint32_t n = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < n; ++i) {
        if (lv_obj_get_child(parent, i) == child) {
            return i;
        }
    }
    return 0;
}

static lv_obj_t *pick_in_direction(lv_obj_t *from, nav_dir_t dir, nav_ctx_t *ctx) {
    if (!from) {
        return NULL;
    }
    const lv_obj_t *root = scroll_root(from);

    const int16_t PROJ_TOL = NAV_ROW_TOLERANCE_PX; // allow reasonable orth overlap tolerance
    const int32_t W_NDD    = 1000;                 // heavy weight on axis distance
    const int32_t W_ORTH   = 10;                   // light penalty for orth deviation

    int64_t best_score = INT64_MAX;
    lv_obj_t *best     = NULL;

    // 1) First pass: projection-limited candidates (tight, preferred)
    for (nav_node_t *n = ctx->head; n; n = n->next) {
        lv_obj_t *to = n->obj;
        if (to == from || !is_focusable(to)) {
            continue;
        }

        // if ((dir == NAV_DIR_UP || dir == NAV_DIR_DOWN) && scroll_root(to) != root) {
        if (scroll_root(to) != root) {
            continue;
        }

        if (!passes_projection(from, to, dir, PROJ_TOL)) {
            continue;
        }

        int32_t ndd, orth, man;
        distances(from, to, dir, &ndd, &orth, &man);
        if (ndd < 0) {
            continue;
        }

        // lane bias if both in same parent and we've remembered index
        int32_t lane_bias    = 0;
        const lv_obj_t *pf   = lv_obj_get_parent(from);
        const lv_obj_t *pt   = lv_obj_get_parent(to);
        const lane_mem_t *lm = (pf == pt) ? lane_get(ctx, pf) : NULL;
        if (lm && lv_obj_get_child(pf, lm->last_index) == to) {
            lane_bias = -5;
        }

        int64_t score = (int64_t)W_NDD * ndd + (int64_t)W_ORTH * orth + man + lane_bias;
        if (score < best_score) {
            best_score = score;
            best       = to;
        }
        // ESP_LOGD(TAG, "pick dir=%d from=%p -> to=%p ndd=%d orth=%d score=%lld", dir, (void *)from, (void *)to, ndd, orth,
        //          (long long)score);
    }

    if (best) {
        return best;
    }

    // 2) Fallback pass: permissive forward half-plane (no projection requirement)
    best_score = INT64_MAX;
    best       = NULL;

    for (nav_node_t *n = ctx->head; n; n = n->next) {
        lv_obj_t *to = n->obj;
        if (to == from || !is_focusable(to)) {
            continue;
        }

        if (scroll_root(to) != root) {
            continue;
        }

        int32_t ndd, orth, man;
        distances(from, to, dir, &ndd, &orth, &man);
        if (ndd < 0) {
            continue; // must be forward along nav axis
        }

        int32_t lane_bias    = 0;
        const lv_obj_t *pf   = lv_obj_get_parent(from);
        const lv_obj_t *pt   = lv_obj_get_parent(to);
        const lane_mem_t *lm = (pf == pt) ? lane_get(ctx, pf) : NULL;
        if (lm && lv_obj_get_child(pf, lm->last_index) == to) {
            lane_bias = -5;
        }

        int64_t score = (int64_t)W_NDD * ndd + (int64_t)W_ORTH * orth + man + lane_bias;
        if (score < best_score) {
            best_score = score;
            best       = to;
        }
        // ESP_LOGD(TAG, "pick dir=%d from=%p -> to=%p ndd=%d orth=%d score=%lld", dir, (void *)from, (void *)to, ndd, orth,
        //          (long long)score);
    }

    return best;
}

// Pick the farthest candidate in the same row so LEFT wraps to rightmost and vice versa
static lv_obj_t *row_wrap_pick(const lv_obj_t *from, nav_dir_t dir, nav_ctx_t *ctx) {
    if (!from) {
        return NULL;
    }
    if (dir != NAV_DIR_LEFT && dir != NAV_DIR_RIGHT) {
        return NULL;
    }

    lv_point_t pf;
    obj_center(from, &pf);

    lv_obj_t *best = NULL;
    int best_x     = 0;

    for (nav_node_t *n = ctx->head; n; n = n->next) {
        lv_obj_t *to = n->obj;
        if (to == from || !is_focusable(to)) {
            continue;
        }

        lv_point_t pt;
        obj_center(to, &pt);
        if (abs(pt.y - pf.y) > NAV_ROW_TOLERANCE_PX) {
            continue;
        }

        if (dir == NAV_DIR_LEFT) {
            if (!best || pt.x > best_x) {
                best   = to;
                best_x = pt.x;
            }
        } else { // NAV_DIR_RIGHT
            if (!best || pt.x < best_x) {
                best   = to;
                best_x = pt.x;
            }
        }
    }
    return best;
}

static void nav_obj_delete_cb(lv_event_t *e) {
    nav_ctx_t *ctx    = lv_event_get_user_data(e);
    const lv_obj_t *o = lv_event_get_target(e);

    nav_node_t **pp = &ctx->head;
    while (*pp) {
        if ((*pp)->obj == o) {
            nav_node_t *dead = *pp;
            *pp              = dead->next;
            free(dead);
            break;
        }
        pp = &(*pp)->next;
    }
}

// ---------------------------------------------------------------------------------------------
// Key Handling
// ---------------------------------------------------------------------------------------------

static void nav_focused_cb(lv_event_t *e) {
    nav_ctx_t *ctx = lv_event_get_user_data(e);
    lv_obj_t *obj  = lv_event_get_target(e);
    if (!ctx || !obj) {
        return;
    }

    LVGL_DEBUG_OBJECT(obj, NULL);

    // if obj has a parent row container, record the index
    const lv_obj_t *parent = lv_obj_get_parent(obj);
    if (!parent) {
        return;
    }

    lane_mem_t *lm = lane_get(ctx, parent);
    if (lm) { // only track rows we opted into
        lm->last_index = child_index_of(parent, obj);
        // Update dynamic vertical bindings anchored on this row
        vbind_refresh_for_upper(ctx, (lv_obj_t *)parent);
    }
}

static void nav_key_cb(lv_event_t *e) {
    nav_ctx_t *ctx = lv_event_get_user_data(e);
    lv_key_t key   = lv_event_get_key(e);
    if (lv_indev_get_group(ctx->keypad) != ctx->group) {
        return;
    }

    // If navigation is suspended, don't process keys but let them bubble up
    if (ctx->suspended) {
        return;
    }

    lv_obj_t *cur = ctx && ctx->group ? lv_group_get_focused(ctx->group) : NULL;
    if (!cur) {
        return;
    }

    bool editing = lv_group_get_editing(ctx->group);
    if (key == LV_KEY_ENTER) {
        if (is_editable_widget(cur)) {
            bool new_edit = !editing;
            lv_group_set_editing(ctx->group, new_edit);
            lv_event_stop_processing(e);
            // return;
        }
        return; // Let ENTER pass through on non-editable widgets
    }
    if (editing) {
        return; // Don't navigate while editing
    }

    nav_dir_t dir;
    switch (key) {
        case LV_KEY_UP: dir = NAV_DIR_UP; break;
        case LV_KEY_DOWN: dir = NAV_DIR_DOWN; break;
        case LV_KEY_LEFT: dir = NAV_DIR_LEFT; break;
        case LV_KEY_RIGHT: dir = NAV_DIR_RIGHT; break;
        default: return;
    }

    lv_event_stop_processing(e);

    if (editing) {
        return; // Don't navigate while editing
    }

    nav_node_t *node = find_node(ctx, cur);
    if (!node) {
        return;
    }

    // If current focus is disabled, move away.
    if (!is_focusable(cur)) {
        ESP_LOGD(TAG, "Current focus %p not focusable, skipping in dir %d", (void *)cur, dir);
        if (!skip_to_focusable_in_group(ctx->group, dir)) {
            return;
        }
        cur  = lv_group_get_focused(ctx->group);
        node = find_node(ctx, cur);
        if (!node) {
            return;
        }
    }

    // Explicitly defined neighbors obviously win
    lv_obj_t *next = node->nbr[dir];
    if (next && !is_focusable(next)) {
        next = NULL;
    }

    // Auto pick by geometry
    if (!next) {
        next = pick_in_direction(cur, dir, ctx);
    }

    // Fallback handling
    if (!next) {
        if (dir == NAV_DIR_LEFT || dir == NAV_DIR_RIGHT) {
            // No lateral candidate, optionally wrap within the same row
            if (ctx->wrap_h) {
                lv_obj_t *wrap = row_wrap_pick(cur, dir, ctx);
                if (wrap) {
                    lv_group_focus_obj(wrap);
                }
            }
            return;
        }

        // Vertical fallback via group prev/next
        if (ctx->vert_fallback) {
            lv_obj_t *old = lv_group_get_focused(ctx->group);
            if (dir == NAV_DIR_UP) {
                lv_group_focus_prev(ctx->group);
            } else {
                lv_group_focus_next(ctx->group);
            }

            lv_obj_t *cur2 = lv_group_get_focused(ctx->group);
            bool same      = (cur2 == old);
            if ((!ctx->wrap_v && same) || !is_focusable(cur2)) {
                if (!is_focusable(cur2)) {
                    if (!skip_to_focusable_in_group(ctx->group, dir)) {
                        lv_group_focus_obj(old);
                    }
                } else {
                    lv_group_focus_obj(old);
                }
            }
        }

        return;
    }

    // ESP_LOGD(TAG, "Navigating from %p to %p in dir %d", (void *)cur, (void *)next, dir);

    lv_group_focus_obj(next);
    lv_obj_scroll_to_view_recursive(next, LV_ANIM_ON);
}

// ---------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------

nav_ctx_t *nav_ctx_create(lv_indev_t *keypad, lv_group_t *group, lv_obj_t *scope_root) {
    if (!keypad || !group || !scope_root) {
        return NULL;
    }

    nav_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate navigation context");
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->keypad        = keypad;
    ctx->group         = group;
    ctx->scope_root    = scope_root;
    ctx->wrap_h        = false;
    ctx->wrap_v        = false;
    ctx->vert_fallback = true;
    ctx->suspended     = false;
    ctx->lanes         = NULL;

    // Make keypad drive this scope
    lv_indev_set_group(keypad, group);

    // Navigation owns group wrap: start disabled
    lv_group_set_wrap(group, false);

    return ctx;
}

void nav_ctx_destroy(nav_ctx_t *ctx) {
    if (!ctx) {
        return;
    }

    // Remove our per-object callbacks
    for (nav_node_t *n = ctx->head; n; n = n->next) {
        if (lv_obj_is_valid(n->obj)) {
            lv_obj_remove_event_cb_with_user_data(n->obj, nav_key_cb, ctx);
            lv_obj_remove_event_cb_with_user_data(n->obj, nav_focused_cb, ctx);
            lv_obj_remove_event_cb_with_user_data(n->obj, nav_obj_delete_cb, ctx);
        }
    }

    // Free nodes
    nav_node_t *n = ctx->head;
    while (n) {
        nav_node_t *next = n->next;
        free(n);
        n = next;
    }

    // Free lane memory
    lane_mem_t *lm = ctx->lanes;
    while (lm) {
        lane_mem_t *next = lm->next;
        free(lm);
        lm = next;
    }

    // Free vertical bindings
    vertical_bind_t *vb = ctx->vbinds;
    while (vb) {
        vertical_bind_t *next = vb->next;
        free(vb);
        vb = next;
    }

    // LVGL will drop event cb when scope_root is deleted
    free(ctx);
}

void nav_ctx_set_wrap(nav_ctx_t *ctx, bool wrap_horiz, bool wrap_vert) {
    if (!ctx) {
        return;
    }
    ctx->wrap_h = wrap_horiz;
    ctx->wrap_v = wrap_vert;

    lv_group_set_wrap(ctx->group, wrap_vert);
}

void nav_ctx_set_vert_fallback(nav_ctx_t *ctx, bool enable) {
    if (!ctx) {
        return;
    }
    ctx->vert_fallback = enable;
}

void nav_suspend(nav_ctx_t *ctx, bool suspend) {
    if (!ctx) {
        return;
    }
    ctx->suspended = suspend;
}

void nav_bind_vertical(nav_ctx_t *ctx, lv_obj_t *upper_row, lv_obj_t *lower_row, bool remember_lane) {
    if (!ctx || !upper_row || !lower_row) {
        return;
    }

    // Record binding for dynamic updates
    vbind_add(ctx, upper_row, lower_row, remember_lane);

    // Register lane memory for both rows if requested
    if (remember_lane) {
        lane_ensure(ctx, upper_row);
    }
    const lane_mem_t *lower_mem = remember_lane ? lane_ensure(ctx, lower_row) : NULL;

    // Ensure all direct children of these rows are registered focusables
    uint32_t up_n = lv_obj_get_child_count(upper_row);
    uint32_t lo_n = lv_obj_get_child_count(lower_row);

    // Special-case: upper_row has no focusable children but is itself focusable (e.g. lone home button)
    // Treat it as a single-item row so vertical navigation still works and lower lane memory applies.
    bool upper_focusable_child_present = false;
    for (uint32_t i = 0; i < up_n && !upper_focusable_child_present; ++i) {
        if (is_focus_target(lv_obj_get_child(upper_row, i))) {
            upper_focusable_child_present = true;
        }
    }
    if (!upper_focusable_child_present && is_focus_target(upper_row)) {
        // Select downward target respecting lower lane memory
        uint32_t target_idx = (lower_mem) ? lower_mem->last_index : 0;
        lv_obj_t *down      = NULL;
        for (uint32_t k = 0; k < lo_n; ++k) {
            uint32_t j  = (target_idx + k < lo_n) ? (target_idx + k) : k;
            lv_obj_t *c = lv_obj_get_child(lower_row, j);
            if (is_focus_target(c)) {
                down = c;
                break;
            }
        }
        if (down) {
            nav_set_neighbor(ctx, upper_row, NAV_DIR_DOWN, down);
            ESP_LOGD(TAG, "nav_bind_vertical(single): set DOWN from %p to %p", (void *)upper_row, (void *)down);
        }
        // Up mapping from each lower child to the lone upper object
        for (uint32_t i = 0; i < lo_n; ++i) {
            lv_obj_t *lo_ch = lv_obj_get_child(lower_row, i);
            if (is_focus_target(lo_ch)) {
                nav_set_neighbor(ctx, lo_ch, NAV_DIR_UP, upper_row);
                if (i == 0) {
                    ESP_LOGD(TAG, "nav_bind_vertical(single): set UP from first lower child %p to %p", (void *)lo_ch,
                             (void *)upper_row);
                }
            }
        }
        return; // Skip standard multi-child logic
    }

    // DOWN from each upper child -> a lower child (standard multi-row behavior)
    for (uint32_t i = 0; i < up_n; ++i) {
        lv_obj_t *up_ch = lv_obj_get_child(upper_row, i);
        if (!is_focus_target(up_ch)) {
            continue;
        }
        uint32_t target_idx = (lower_mem) ? lower_mem->last_index : 0;
        lv_obj_t *down      = NULL;
        for (uint32_t k = 0; k < lo_n; ++k) {
            uint32_t j  = (target_idx + k < lo_n) ? (target_idx + k) : k;
            lv_obj_t *c = lv_obj_get_child(lower_row, j);
            if (is_focus_target(c)) {
                down = c;
                break;
            }
        }
        if (down) {
            nav_set_neighbor(ctx, up_ch, NAV_DIR_DOWN, down);
        }
    }
    vbind_refresh_for_upper(ctx, upper_row);
}

void nav_register(nav_ctx_t *ctx, lv_obj_t *obj) {
    if (!ctx || !obj) {
        return;
    }

    nav_node_t *n = malloc(sizeof(*n));
    if (!n) {
        ESP_LOGE(TAG, "Failed to allocate navigation node");
        return;
    }

    memset(n, 0, sizeof(*n));
    n->obj    = obj;
    n->next   = ctx->head;
    ctx->head = n;

    lv_group_add_obj(ctx->group, obj);
    lv_obj_add_event_cb(obj, nav_key_cb, LV_EVENT_KEY, ctx);
    lv_obj_add_event_cb(obj, nav_focused_cb, LV_EVENT_FOCUSED, ctx);
    lv_obj_add_event_cb(obj, nav_obj_delete_cb, LV_EVENT_DELETE, ctx);
}

void nav_register_children(nav_ctx_t *ctx, const lv_obj_t *container) {
    if (!ctx || !container) {
        return;
    }

    uint32_t cnt = lv_obj_get_child_count(container);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *ch = lv_obj_get_child(container, i);
        nav_register(ctx, ch);
    }
}

void nav_register_descendants(nav_ctx_t *ctx, lv_obj_t *root) {
    if (!ctx || !root) {
        return;
    }

    // Register this object if focusable
    if (is_focus_target(root)) {
        nav_register(ctx, root);
    }

    // Recurse into children
    uint32_t cnt = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *ch = lv_obj_get_child(root, i);
        nav_register_descendants(ctx, ch);
    }
}

void nav_set_neighbor(nav_ctx_t *ctx, lv_obj_t *obj, nav_dir_t dir, lv_obj_t *target) {
    if (!ctx || !obj || dir >= NAV_DIR_COUNT) {
        return;
    }

    nav_node_t *n = find_node(ctx, obj);
    if (!n) {
        nav_register(ctx, obj);
        n = find_node(ctx, obj);
    }
    n->nbr[dir] = target;
}

void nav_autolink(nav_ctx_t *ctx) {
    if (!ctx) {
        return;
    }

    for (nav_node_t *n = ctx->head; n; n = n->next) {
        for (int d = 0; d < NAV_DIR_COUNT; d++) {
            if (!n->nbr[d]) {
                n->nbr[d] = pick_in_direction(n->obj, (nav_dir_t)d, ctx);
            }
        }
    }
}

void nav_focus(nav_ctx_t *ctx, lv_obj_t *obj) {
    if (ctx && obj) {
        ESP_LOGD(TAG, "nav_focus: obj=%p", (void *)obj);
        lv_group_focus_obj(obj);
        lv_obj_scroll_to_view_recursive(obj, LV_ANIM_ON);
    }
}

lv_obj_t *nav_get_focused(nav_ctx_t *ctx) {
    if (ctx && ctx->group) {
        return lv_group_get_focused(ctx->group);
    }
    return NULL;
}

// ---------------------------------------------------------------------------------------------
// Navigation scope push/pop
// ---------------------------------------------------------------------------------------------

nav_scope_guard_t nav_scope_push(lv_indev_t *keypad, lv_group_t *group) {
    nav_scope_guard_t g = {0};
    g.prev_group        = lv_indev_get_group(keypad);
    g.prev_focus        = g.prev_group ? lv_group_get_focused(g.prev_group) : NULL;
    if (g.prev_group) {
        lv_group_set_editing(g.prev_group, false);
    }
    lv_group_set_editing(group, false);
    lv_indev_set_group(keypad, group);
    return g;
}

void nav_scope_pop(lv_indev_t *keypad, nav_scope_guard_t g) {
    lv_group_t *group = lv_indev_get_group(keypad);
    if (group) {
        lv_group_set_editing(group, false);
    }
    lv_indev_set_group(keypad, g.prev_group);
    if (g.prev_group) {
        lv_group_set_editing(g.prev_group, false);
        if (g.prev_focus) {
            lv_group_focus_obj(g.prev_focus);
        }
    }
}