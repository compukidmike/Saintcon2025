#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

// Directions as bits so you can combine if needed
typedef enum { //
    NAV_DIR_UP    = 0,
    NAV_DIR_RIGHT = 1,
    NAV_DIR_DOWN  = 2,
    NAV_DIR_LEFT  = 3,
    NAV_DIR_COUNT = 4
} nav_dir_t;

typedef struct nav_node_t nav_node_t;

typedef struct lane_mem_t {
    lv_obj_t *container;
    uint32_t last_index;
    struct lane_mem_t *next;
} lane_mem_t;

// Forward decl for internal vertical binding list (defined privately in nav.c)
typedef struct vertical_bind_t vertical_bind_t;

typedef struct {
    lv_group_t *group;       // focus scope
    lv_indev_t *keypad;      // keypad indev
    lv_obj_t *scope_root;    // where we attach the key handler
    nav_node_t *head;        // linked list of nodes
    bool wrap_h;             // allow left/right wrap in linear seq
    bool wrap_v;             // allow up/down wrap
    bool vert_fallback;      // allow vertical fallback via group order
    bool suspended;          // temporarily disable automatic navigation
    lane_mem_t *lanes;       // simpler per-container lane memory
    vertical_bind_t *vbinds; // vertical row bindings (upper<->lower) for dynamic neighbor updates
} nav_ctx_t;

typedef struct {
    lv_group_t *prev_group;
    lv_obj_t *prev_focus;
} nav_scope_guard_t;

/**
 * @brief Create a navigation context
 *
 * @param keypad The keypad input device
 * @param group The LVGL group for focus management
 * @param scope_root The root object for the navigation scope
 * @return nav_ctx_t*
 */
nav_ctx_t *nav_ctx_create(lv_indev_t *keypad, lv_group_t *group, lv_obj_t *scope_root);

/**
 * @brief Destroy a navigation context
 *
 * @param ctx The navigation context to destroy
 */
void nav_ctx_destroy(nav_ctx_t *ctx);

/**
 * @brief Set wrap behavior for navigation
 *
 * @param ctx The navigation context
 * @param wrap_horiz Enable horizontal wrapping
 * @param wrap_vert Enable vertical wrapping
 */
void nav_ctx_set_wrap(nav_ctx_t *ctx, bool wrap_horiz, bool wrap_vert);

/**
 * @brief Enable or disable vertical fallback for navigation
 *
 * @param ctx The navigation context
 * @param enable Enable vertical fallback
 */
void nav_ctx_set_vert_fallback(nav_ctx_t *ctx, bool enable);

/**
 * @brief Suspend or resume automatic navigation
 *
 * When suspended, navigation key events are not processed automatically,
 * allowing custom handling. Events will still bubble up normally.
 *
 * @param ctx The navigation context
 * @param suspend True to suspend automatic navigation, false to resume
 */
void nav_suspend(nav_ctx_t *ctx, bool suspend);

/**
 * @brief Bind vertical navigation between two rows
 *
 * @param ctx The navigation context
 * @param upper_row The upper row object
 * @param lower_row The lower row object
 * @param remember_lane Whether to remember the lane
 */
void nav_bind_vertical(nav_ctx_t *ctx, lv_obj_t *upper_row, lv_obj_t *lower_row, bool remember_lane);

/**
 * @brief Register a focusable object
 *
 * @param ctx The navigation context
 * @param obj The object to register
 */
void nav_register(nav_ctx_t *ctx, lv_obj_t *obj);

/**
 * @brief Register all direct children of a container
 *
 * @param ctx The navigation context
 * @param container The container object
 */
void nav_register_children(nav_ctx_t *ctx, const lv_obj_t *container);

/**
 * @brief Recursively register focusable descendants of a root
 *
 * @param ctx The navigation context
 * @param root The root object
 */
void nav_register_descendants(nav_ctx_t *ctx, lv_obj_t *root);

/**
 * @brief Set explicit neighbor for navigation
 *
 * @param ctx The navigation context
 * @param obj The object to modify
 * @param dir The direction of the neighbor
 * @param target The neighbor object (NULL to clear)
 */
void nav_set_neighbor(nav_ctx_t *ctx, lv_obj_t *obj, nav_dir_t dir, lv_obj_t *target);

/**
 * @brief Compute automatic neighbors for any unset direction using spatial heuristic
 *
 * @param ctx The navigation context
 */
void nav_autolink(nav_ctx_t *ctx);

/**
 * @brief Set focus on a specific object
 *
 * @param ctx The navigation context
 * @param obj The object to focus
 */
void nav_focus(nav_ctx_t *ctx, lv_obj_t *obj);

/**
 * @brief Get the currently focused object in the navigation context
 *
 * @param ctx The navigation context
 * @return lv_obj_t* The currently focused object, or NULL if none
 */
lv_obj_t *nav_get_focused(nav_ctx_t *ctx);

/**
 * @brief Push a new navigation scope
 *
 * @param keypad The keypad input device
 * @param group The LVGL group for the modal
 * @return nav_scope_guard_t
 */
nav_scope_guard_t nav_scope_push(lv_indev_t *keypad, lv_group_t *group);

/**
 * @brief Pop the current navigation scope
 *
 * @param keypad The keypad input device
 * @param guard The scope guard to pop
 */
void nav_scope_pop(lv_indev_t *keypad, nav_scope_guard_t guard);

#ifdef __cplusplus
}
#endif
