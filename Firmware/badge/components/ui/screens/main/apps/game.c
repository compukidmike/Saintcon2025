#include "esp_log.h"

#include "badge_game.h"
#include "game.h"
#include "theme.h"
#include <math.h>
#include <stdbool.h>

static const char *TAG = "screens/main [apps/badge_game]";

#define NODE_RADIUS   12
#define RING_SPACING  (NODE_RADIUS * 3)
#define CANVAS_WIDTH  240
#define CANVAS_HEIGHT (320 - 40) // Account for status bar height

// Canvas for drawing the game map
static lv_obj_t *canvas             = NULL;
static lv_draw_buf_t *canvas_buffer = NULL;
static lv_timer_t *refresh_timer    = NULL;

// Forward declarations
static lv_point_t get_node_position(int node_id);
static void refresh_timer_cb(lv_timer_t *timer);
static void draw_connections(const game_state_t *state);
static void draw_six_pointed_star(lv_layer_t *layer, lv_point_t center, int radius, lv_color_t fill_color,
                                  lv_color_t border_color, int border_width, bool rotate_between_connections);
static void draw_triangle_with_border(lv_layer_t *layer, lv_point_precise_t points[3], lv_color_t fill_color,
                                      lv_color_t border_color, int border_width);
static void redraw_canvas_with_state(const game_state_t *state);
static void redraw_canvas();
static void game_app_cleanup(lv_event_t *e);

void game_app_create(lv_obj_t *parent) {
    // Initialize badge game system
    badge_game_init();

    // Create a buffer to hold the canvas data - we need to allocate dynamically due to internal memory constraints
    canvas_buffer = lv_draw_buf_create(CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_ARGB8888, 0);
    if (canvas_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to create canvas buffer");
        return;
    }

    canvas = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(canvas, canvas_buffer);
    lv_obj_set_size(canvas, CANVAS_WIDTH, CANVAS_HEIGHT);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(canvas, game_app_cleanup, LV_EVENT_DELETE, NULL);

    // Immediately render the (most likely fallback) state
    const game_state_t *game_state = badge_game_get_state();
    if (game_state != NULL) {
        redraw_canvas_with_state(game_state);
    } else {
        ESP_LOGE(TAG, "No valid game state available during initialization");
    }

    // Force display update to render immediately
    lv_obj_invalidate(canvas);
    lv_refr_now(NULL);

    // Start periodic refresh timer (every 30 seconds)
    if (refresh_timer == NULL) {
        refresh_timer = lv_timer_create(refresh_timer_cb, 30000, NULL);
    }

    // Start initial API refresh in background
    game_app_refresh();
}

void game_app_refresh() {
    if (canvas == NULL) {
        ESP_LOGW(TAG, "Canvas not initialized, cannot refresh");
        return;
    }

    ESP_LOGI(TAG, "Refreshing game data from API...");

    // Update from API in background
    if (badge_game_update_state()) {
        ESP_LOGI(TAG, "Game data updated successfully, redrawing with live data...");
        redraw_canvas(); // Redraw with updated API data
    } else {
        ESP_LOGE(TAG, "Failed to update game data from API - keeping basic grid");
        // Basic grid is already rendered, no need to change anything
    }
}

// Timer callback for periodic API refresh
static void refresh_timer_cb(lv_timer_t *timer) {
    ESP_LOGI(TAG, "Periodic game data refresh triggered");
    game_app_refresh();
}

// Calculate node position based on ID
static lv_point_t get_node_position(int node_id) {
    const int cx = CANVAS_WIDTH / 2;
    const int cy = CANVAS_HEIGHT / 2;

    // We can tweak these for positioning if needed
    const float r_1     = RING_SPACING * 1.00f; // ring 1 radius
    const float r_2     = RING_SPACING * 2.00f; // ring 2 radius
    const float r_tower = RING_SPACING * 2.95f; // tower ring radius
    const float r_hq    = RING_SPACING * 3.30f; // HQ ring radius

    static const float hq_angles[6]    = {120.0f, 60.0f, 0.0f, -60.0f, -120.0f, 180.0f};
    static const float tower_angles[6] = {90.0f, 30.0f, -30.0f, -90.0f, -150.0f, 150.0f};
    float angle, radius;

    lv_point_t pos = {cx, cy};

    // Core node
    if (node_id == 1) {
        return pos;
    }

    // Ring 1: 6 nodes, 60° spacing
    if (node_id >= 2 && node_id <= 7) {
        int ring_index = node_id - 2;
        angle          = (90.0f - 60.0f * ring_index) * M_PI / 180.0f;
        radius         = r_1;
    }
    // Ring 2: 12 nodes, 30° spacing
    else if (node_id >= 8 && node_id <= 19) {
        int ring_index = node_id - 8;
        angle          = (90.0f - 30.0f * ring_index) * M_PI / 180.0f;
        radius         = r_2;
    }
    // HQ nodes
    else if (node_id >= 20 && node_id <= 25) {
        angle  = hq_angles[node_id - 20] * M_PI / 180.0f;
        radius = r_hq;
    }
    // Tower nodes
    else if (node_id >= 26 && node_id <= 31) {
        angle  = tower_angles[node_id - 26] * M_PI / 180.0f;
        radius = r_tower;
    }

    // Anything else is unexpected and should be an error
    else {
        ESP_LOGE(TAG, "Invalid node_id %d in get_node_position", node_id);
        return pos;
    }

    pos.x = cx + (int)lroundf(radius * cosf(angle));
    pos.y = cy - (int)lroundf(radius * sinf(angle));

    return pos;
}

// Helper function to get port status for a specific node and port
static game_link_status_t get_port_status(const game_state_t *state, int node_id, int port_number) {
    if (state == NULL || state->port_status == NULL) {
        return LINK_STATUS_UNKNOWN;
    }

    for (int i = 0; i < state->port_status_count; i++) {
        if (state->port_status[i].node_id == node_id && state->port_status[i].port_number == port_number) {
            return state->port_status[i].link_status;
        }
    }
    return LINK_STATUS_UNKNOWN;
}

static void draw_connections(const game_state_t *state) {
    if (state == NULL || state->connection_count == 0) {
        return;
    }

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.width = 2; // Slightly thicker for better visibility

    for (int i = 0; i < state->connection_count; i++) {
        const game_connection_info_t *conn = &state->connections[i];

        // Find the nodes for this connection
        const game_node_info_t *node_a = badge_game_get_node(conn->node_a);
        const game_node_info_t *node_b = badge_game_get_node(conn->node_b);

        if (node_a != NULL && node_b != NULL) {
            // Calculate the correct positions
            lv_point_t pos_a = get_node_position(node_a->id);
            lv_point_t pos_b = get_node_position(node_b->id);

            // Get port status for both ends of the connection
            const game_link_status_t status_a = get_port_status(state, conn->node_a, conn->node_a_port);
            const game_link_status_t status_b = get_port_status(state, conn->node_b, conn->node_b_port);

            // Calculate midpoint for potential partial connections
            lv_point_t midpoint = {(pos_a.x + pos_b.x) / 2, (pos_a.y + pos_b.y) / 2};

            // Always draw the full gray line first as background
            line_dsc.color = lv_color_hex(GRAY_TINT_6);
            line_dsc.opa   = LV_OPA_30;
            line_dsc.p1    = (lv_point_precise_t){pos_a.x, pos_a.y};
            line_dsc.p2    = (lv_point_precise_t){pos_b.x, pos_b.y};
            lv_draw_line(&layer, &line_dsc);

            // Check if both ends have FULL connection - draw full green line
            if (status_a == LINK_STATUS_FULL && status_b == LINK_STATUS_FULL) {
                line_dsc.color = lv_color_hex(0x00FF00); // Green
                line_dsc.opa   = LV_OPA_80;
                line_dsc.p1    = (lv_point_precise_t){pos_a.x, pos_a.y};
                line_dsc.p2    = (lv_point_precise_t){pos_b.x, pos_b.y};
                lv_draw_line(&layer, &line_dsc);
            }
            // Draw partial connections if present
            else {
                // Node A side - draw green/yellow from A to midpoint if connected
                if (status_a == LINK_STATUS_FULL || status_a == LINK_STATUS_PARTIAL) {
                    line_dsc.color = status_a == LINK_STATUS_FULL ? lv_color_hex(0x00FF00) : lv_color_hex(0xFFFF00);
                    line_dsc.opa   = LV_OPA_80;
                    line_dsc.p1    = (lv_point_precise_t){pos_a.x, pos_a.y};
                    line_dsc.p2    = (lv_point_precise_t){midpoint.x, midpoint.y};
                    lv_draw_line(&layer, &line_dsc);
                }
                // Node B side - draw green/yellow from midpoint to B if connected
                if (status_b == LINK_STATUS_FULL || status_b == LINK_STATUS_PARTIAL) {
                    line_dsc.color = status_b == LINK_STATUS_FULL ? lv_color_hex(0x00FF00) : lv_color_hex(0xFFFF00);
                    line_dsc.opa   = LV_OPA_80;
                    line_dsc.p1    = (lv_point_precise_t){midpoint.x, midpoint.y};
                    line_dsc.p2    = (lv_point_precise_t){pos_b.x, pos_b.y};
                    lv_draw_line(&layer, &line_dsc);
                }
            }
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static void draw_six_pointed_star(lv_layer_t *layer, lv_point_t center, int radius, lv_color_t fill_color,
                                  lv_color_t border_color, int border_width, bool rotate) {
    // Calculate the 6 outer and 6 inner points of the star
    lv_point_precise_t star_points[12];
    int outer_radius = radius;
    int inner_radius = radius * 0.5f;

    // Rotation offset: 0° for alignment with connections, 30° to place between connections
    float rotation_offset = rotate ? 30.0f : 0.0f;

    for (int i = 0; i < 12; i++) {
        float angle = (i * 30.0f - 90.0f + rotation_offset) * M_PI / 180.0f;
        int r       = (i % 2 == 0) ? outer_radius : inner_radius;

        star_points[i].x = center.x + r * cos(angle);
        star_points[i].y = center.y + r * sin(angle);
    }

    // Triangle structure to define the points
    lv_draw_triangle_dsc_t fill_dsc;
    lv_draw_triangle_dsc_init(&fill_dsc);
    fill_dsc.color = fill_color;
    fill_dsc.opa   = LV_OPA_COVER;

    // Draw the 6 points using triangles
    for (int i = 0; i < 12; i += 2) {
        fill_dsc.p[0] = star_points[i];             // outer point
        fill_dsc.p[1] = star_points[(i + 1) % 12];  // inner point (clockwise)
        fill_dsc.p[2] = star_points[(i + 11) % 12]; // inner point (counter-clockwise)
        lv_draw_triangle(layer, &fill_dsc);
    }

    // Draw a circle in the center to fill the inner area
    lv_draw_rect_dsc_t center_dsc;
    lv_draw_rect_dsc_init(&center_dsc);
    center_dsc.bg_color = fill_color;
    center_dsc.bg_opa   = LV_OPA_COVER;
    center_dsc.radius   = LV_RADIUS_CIRCLE;

    lv_draw_rect(layer, &center_dsc,
                 &(lv_area_t){.x1 = center.x - inner_radius,
                              .y1 = center.y - inner_radius,
                              .x2 = center.x + inner_radius,
                              .y2 = center.y + inner_radius});

    // Line structure for the border
    lv_draw_line_dsc_t border_line_dsc;
    lv_draw_line_dsc_init(&border_line_dsc);
    border_line_dsc.color = border_color;
    border_line_dsc.width = border_width;
    border_line_dsc.opa   = LV_OPA_COVER;

    // Draw the star outline by connecting all 12 points
    for (int i = 0; i < 12; i++) {
        border_line_dsc.p1 = star_points[i];
        border_line_dsc.p2 = star_points[(i + 1) % 12];
        lv_draw_line(layer, &border_line_dsc);
    }
}

static void draw_triangle_with_border(lv_layer_t *layer, lv_point_precise_t points[3], lv_color_t fill_color,
                                      lv_color_t border_color, int border_width) {
    // Calculate the center of the triangle
    lv_point_precise_t center = {(points[0].x + points[1].x + points[2].x) / 3.0f,
                                 (points[0].y + points[1].y + points[2].y) / 3.0f};

    // Create an expanded triangle by scaling outward from center by border width to create a border
    lv_point_precise_t border_points[3];
    for (int i = 0; i < 3; i++) {
        // Vector from center to point
        float dx     = points[i].x - center.x;
        float dy     = points[i].y - center.y;
        float length = sqrtf(dx * dx + dy * dy);

        if (length > 0) {
            // Normalize and extend by border width (rounding up so the border doesn't look thinner than expected)
            dx = (dx / length) * (border_width + 0.5f);
            dy = (dy / length) * (border_width + 0.5f);
        }

        border_points[i].x = points[i].x + dx;
        border_points[i].y = points[i].y + dy;
    }

    // Draw border triangle first
    lv_draw_triangle_dsc_t border_dsc;
    lv_draw_triangle_dsc_init(&border_dsc);
    border_dsc.color = border_color;
    border_dsc.p[0]  = border_points[0];
    border_dsc.p[1]  = border_points[1];
    border_dsc.p[2]  = border_points[2];
    border_dsc.opa   = LV_OPA_COVER;
    lv_draw_triangle(layer, &border_dsc);

    // Draw fill triangle on top
    lv_draw_triangle_dsc_t fill_dsc;
    lv_draw_triangle_dsc_init(&fill_dsc);
    fill_dsc.color = fill_color;
    fill_dsc.p[0]  = points[0];
    fill_dsc.p[1]  = points[1];
    fill_dsc.p[2]  = points[2];
    fill_dsc.opa   = LV_OPA_COVER;
    lv_draw_triangle(layer, &fill_dsc);
}

static void redraw_canvas_with_state(const game_state_t *state) {
    if (canvas == NULL || state == NULL) {
        return;
    }

    // Clear canvas
    lv_canvas_fill_bg(canvas, lv_color_hex(WHITE), LV_OPA_TRANSP);

    // Draw connections first (so they appear behind nodes)
    draw_connections(state);

    // Draw nodes
    for (int i = 0; i < state->node_count; i++) {
        const game_node_info_t *node = &state->nodes[i];
        // Use mathematical positioning instead of API coordinates
        lv_point_t screen_pos = get_node_position(node->id);

        lv_layer_t layer;
        lv_canvas_init_layer(canvas, &layer);

        lv_color_t fill        = lv_color_hex(GRAY_TINT_6);
        lv_color_t border      = lv_color_hex(BLACK);
        lv_color_t text        = lv_color_hex(BLACK);
        lv_point_t text_offset = {0, 0};

        // Set colors based on node faction/team
        if (node->faction != FACTION_NONE) {
            const faction_t faction_data = get_faction(node->faction);
            fill                         = faction_data.screen_color;
            // if the fill is too dark, use a light text color
            if (lv_color_brightness(fill) < 128) {
                text = lv_color_hex(WHITE);
            }
        }

        // Draw a 6-pointed star for the core node
        if (node->type == NODE_TYPE_CORE) {
            fill = lv_color_hex(0xDAA06D);
            draw_six_pointed_star(&layer, screen_pos, NODE_RADIUS + 6, fill, border, 2, true);
        }
        // Draw a triangle for towers
        else if (node->type == NODE_TYPE_TOWER) {
            int radius = NODE_RADIUS + 2;
            fill       = lv_color_hex(GRAY_SHADE_3);
            border     = lv_color_hex(GRAY_TINT_5);

            if (node->faction != FACTION_NONE) {
                border = get_faction(node->faction).screen_color;
            }

            if (lv_color_brightness(fill) < 128) {
                text = lv_color_hex(WHITE);
            }
            text_offset = (lv_point_t){0, 7}; // Adjust the text vertically to not get clipped by the edge of the triangle
            lv_point_precise_t triangle_points[] = {{screen_pos.x, screen_pos.y - radius},
                                                    {screen_pos.x - (radius - 3), screen_pos.y + radius},
                                                    {screen_pos.x + (radius - 3), screen_pos.y + radius}};
            draw_triangle_with_border(&layer, triangle_points, fill, border, 3);
        }
        // Make a circle for all other nodes
        else {
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color     = fill;
            rect_dsc.bg_opa       = LV_OPA_COVER;
            rect_dsc.border_color = border;
            rect_dsc.border_width = 2;
            rect_dsc.radius       = LV_RADIUS_CIRCLE;
            lv_draw_rect(&layer, &rect_dsc,
                         &(lv_area_t){.x1 = screen_pos.x - NODE_RADIUS,
                                      .y1 = screen_pos.y - NODE_RADIUS,
                                      .x2 = screen_pos.x + NODE_RADIUS,
                                      .y2 = screen_pos.y + NODE_RADIUS});
        }

        // Add a label with the node number
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        char num_str[4];
        snprintf(num_str, sizeof(num_str), "%d", node->id);
        label_dsc.text  = num_str;
        label_dsc.font  = &lv_font_montserrat_12;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        label_dsc.color = text;

        lv_point_t txt_size;
        lv_text_get_size(&txt_size, num_str, label_dsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_draw_label(&layer, &label_dsc,
                      &(lv_area_t){.x1 = screen_pos.x - txt_size.x / 2 + text_offset.x,
                                   .y1 = screen_pos.y - txt_size.y / 2 + text_offset.y,
                                   .x2 = screen_pos.x + txt_size.x / 2 - text_offset.x,
                                   .y2 = screen_pos.y + txt_size.y / 2 - text_offset.y});

        lv_canvas_finish_layer(canvas, &layer);
    }
}

static void redraw_canvas() {
    const game_state_t *state = badge_game_get_state();
    if (state == NULL || !state->is_valid) {
        ESP_LOGW(TAG, "No valid game state available");
        return;
    }
    redraw_canvas_with_state(state);
}

static void game_app_cleanup(lv_event_t *e) {
    // Stop the periodic refresh timer
    if (refresh_timer != NULL) {
        lv_timer_delete(refresh_timer);
        refresh_timer = NULL;
    }

    if (canvas_buffer != NULL) {
        lv_draw_buf_destroy(canvas_buffer);
        canvas_buffer = NULL;
    }
    canvas = NULL;
    badge_game_cleanup();
}