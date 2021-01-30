/**
 * fatt.h
 *
 */

static void populate_task_list_entries(Mode *sw);

static int fatt_mode_init(Mode *sw);

static unsigned int fatt_mode_get_num_entries(const Mode *sw);

static ModeMode fatt_mode_result(Mode *sw, int mretv, char **input, unsigned int selected_line);

static void fatt_mode_destroy(Mode *sw);

static char *fatt_get_display_value(const Mode *sw, unsigned int selected_line, G_GNUC_UNUSED int *state, G_GNUC_UNUSED GList **attr_list, int get_entry);

static int fatt_token_match(const Mode *sw, rofi_int_matcher **tokens, unsigned int index);

static char *fatt_get_message(const Mode *sw);