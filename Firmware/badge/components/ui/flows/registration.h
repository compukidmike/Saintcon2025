#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Opens registration modal only if not already open and not yet registered.
void check_registration();

// Opens a modal for player name input with provided title string.
// Title must be a static or persistent string; it is not copied.
void open_player_name_modal(const char *title, const char *button_text);

#ifdef __cplusplus
}
#endif
