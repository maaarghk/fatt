/**
 * rofi-plugin-fatt
 *
 * A rofi plugin to do FreeAgent Time Tracking
 */

#include <stdio.h>
#include <gmodule.h>
#include <rofi/mode.h>
#include <rofi/helper.h>
#include <rofi/mode-private.h>

#include "fatt.h"
#include "external_process.h"

// A bunch of stuff in rofi that we need but is not part of the header files...
typedef struct RofiViewState RofiViewState;
extern RofiViewState *rofi_view_get_active(void);
extern void rofi_view_reload(void);
extern void rofi_view_clear_input(RofiViewState *state);
extern void rofi_view_set_selected_line(RofiViewState *state, unsigned int selected_line);

enum FATTMode
{
    FATT_MODE_ERROR_DISPLAY,
    FATT_MODE_TASK_LIST,
    FATT_MODE_TIMESLIP_LIST
};

enum FATTRowType
{
    FATT_ROW_TASK,
    FATT_ROW_TIMESLIP,
    FATT_ROW_TIMESLIP_RUNNING,
    FATT_ROW_ERROR
};

typedef struct
{
    enum FATTRowType type;
    char *taskId;
    char *timeslipId;
    char *name;
} FATTRow;

/**
 * The internal data structure holding the private data of the FATT Mode.
 */
typedef struct
{
    enum FATTMode currentMode;
    char *relatedTaskId; // Used for custom input (row not available) in timeslip mode
    GPtrArray *taskList;
    GPtrArray *rows;
} FATTModePrivateData;

G_MODULE_EXPORT Mode mode;

void fatt_task_list_cb(GString *buffer, Mode *sw)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);
    FATTRow *newRow;

    if (buffer == NULL) {
        return;
    }

    // Split stdout into lines and insert each into the list
    char **output_lines = g_strsplit(buffer->str, "\n", -1);
    int i = 0;
    while (output_lines[i]) {
        g_strstrip(output_lines[i]);
        if (*output_lines[i] != '\0') {
            // Tasks return two columns, separated by tabs. First is the ID and
            // second is the name.
            char **output_cols = g_strsplit(output_lines[i], "\t", -1);
            newRow = g_new(FATTRow, 1);
            newRow->taskId = output_cols[0];
            newRow->name = output_cols[1];
            newRow->type = FATT_ROW_TASK;
            newRow->timeslipId = NULL;
            g_ptr_array_add(pd->rows, newRow);
            g_ptr_array_add(pd->taskList, newRow);
        }
        i++;
    }
    g_strfreev(output_lines);
    g_string_free(buffer, TRUE);

    rofi_view_reload();
}

void fatt_timeslip_list_cb(GString *buffer, Mode *sw)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);
    FATTRow *newRow;

    if (buffer == NULL) {
        return;
    }

    // Split stdout into lines and insert each into the list
    char **output_lines = g_strsplit(buffer->str, "\n", -1);
    int i = 0;
    while (output_lines[i]) {
        g_strstrip(output_lines[i]);
        if (*output_lines[i] != '\0') {
            // Timeslips return up to 4 tab separated columns. First two are ID
            // and Name. The third is the related task ID, to help us go back to
            // a task list from the timer screen. The fourth is optional, it is
            // an indicator of whether the task is currently recording time.
            char **output_cols = g_strsplit(output_lines[i], "\t", -1);
            newRow = g_new(FATTRow, 1);
            newRow->timeslipId = output_cols[0];
            newRow->name = output_cols[1];
            newRow->taskId = output_cols[2];
            newRow->type = FATT_ROW_TIMESLIP;
            if (output_cols[3] != NULL) {
                newRow->type = FATT_ROW_TIMESLIP_RUNNING;
            }
            g_ptr_array_add(pd->rows, newRow);
        }
        i++;
    }
    g_strfreev(output_lines);
    g_string_free(buffer, TRUE);

    rofi_view_reload();
}

static void reset_view(Mode *sw)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);

    // Reset the view - empty, rows, scroll to line 0 and clear text box
    pd->rows = g_ptr_array_new();
    pd->relatedTaskId = NULL;
    RofiViewState *state = rofi_view_get_active();
    rofi_view_set_selected_line(state, 0);
    rofi_view_clear_input(state);
}

/**
 * Get the entries to display when in TASK_LIST mode
 */
static void populate_task_list_entries(Mode *sw)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);

    pd->currentMode = FATT_MODE_TASK_LIST;
    pd->relatedTaskId = NULL;
    // Use cached data
    if (pd->taskList->len != 0) {
        pd->rows = pd->taskList;
        return;
    }

    ExternalProcess *externalProcessTimers = external_process_init();
    char *argvTimers[] = {"freeagent", "get-running-timers", NULL};
    external_process_launch(externalProcessTimers, NULL, argvTimers, (ExternalProcessCallback)fatt_timeslip_list_cb, sw);

    ExternalProcess *externalProcessTasklist = external_process_init();
    char *argvTasks[] = {"freeagent", "get-task-list", NULL};
    external_process_launch(externalProcessTasklist, NULL, argvTasks, (ExternalProcessCallback)fatt_task_list_cb, sw);
}

/**
 * Get the entries to display when in TIMESLIP mode
 */
static void populate_time_slip_entries(Mode *sw, char *id)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);

    // Reset view blanks the textbox, which is important since if we came here
    // from loading a task, we might have filtered the task list, and if we came
    // here from entering a new timeslip, we probably want to see the rest of
    // timeslip list. In that case, the new timeslip will also be the top row,
    // and will be selected, so we can quickly stop the timer if we want to.
    reset_view(sw);

    // Set Timeslip mode and call row generator
    pd->currentMode = FATT_MODE_TIMESLIP_LIST;
    pd->relatedTaskId = strdup(id);
    ExternalProcess *externalProcess = external_process_init();
    char *argv[] = {"freeagent", "get-timeslip-list", id, NULL};
    external_process_launch(externalProcess, NULL, argv, (ExternalProcessCallback)fatt_timeslip_list_cb, sw);
}

/**
* Called on startup when enabled (in modi list)
*/
static int fatt_mode_init(Mode *sw)
{
    if (mode_get_private_data(sw) == NULL) {
        FATTModePrivateData *pd = g_new0(FATTModePrivateData, 1);
        pd->rows = g_ptr_array_new();
        pd->taskList = g_ptr_array_new();
        mode_set_private_data(sw, (void *)pd);
        // Load content.
        populate_task_list_entries(sw);
    }
    return TRUE;
}

static unsigned int fatt_mode_get_num_entries(const Mode *sw)
{
    const FATTModePrivateData *pd = (const FATTModePrivateData *)mode_get_private_data(sw);
    return pd->rows->len;
}

static ModeMode fatt_mode_result(Mode *sw, int mretv, char **input, unsigned int selected_line)
{
    ModeMode retv = MODE_EXIT;
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);

    if (mretv & MENU_CANCEL)
    {
        // Esc goes back to task list if looking at timeslips
        if (pd->currentMode == FATT_MODE_TIMESLIP_LIST) {
            pd->rows = g_ptr_array_new();
            populate_task_list_entries(sw);
            retv = RELOAD_DIALOG;
        }
    }
    else if ((mretv & MENU_OK))
    {
        FATTRow *selectedRow = g_ptr_array_index(pd->rows, selected_line);

        // If current row is a TASK, replace list with timeslips belonging to current task
        if (selectedRow->type == FATT_ROW_TASK) {
            populate_time_slip_entries(sw, selectedRow->taskId);
            retv = RELOAD_DIALOG;
        } else if (selectedRow->type == FATT_ROW_TIMESLIP) {
            // If current row is a TIMESLIP, start timer
            char *commandLine;
            printf("Start timer on timeslip %s\n", selectedRow->timeslipId);
            commandLine = g_strdup_printf("freeagent start-timer %s", selectedRow->timeslipId);
            g_spawn_command_line_async(commandLine, NULL);
            selectedRow->type = FATT_ROW_TIMESLIP_RUNNING;
            retv = RELOAD_DIALOG;
        } else if (selectedRow->type == FATT_ROW_TIMESLIP_RUNNING) {
            // If current row is a TIMESLIP_RUNNING, stop timer
            char *commandLine;
            printf("Stop timer on timeslip %s\n", selectedRow->timeslipId);
            commandLine = g_strdup_printf("freeagent stop-timer %s", selectedRow->timeslipId);
            g_spawn_command_line_async(commandLine, NULL);
            selectedRow->type = FATT_ROW_TIMESLIP;
            retv = RELOAD_DIALOG;
        }
    }
    else if ((mretv & MENU_CUSTOM_INPUT) && *input)
    {
        // If on timeslip mode, create a new timeslip in the same task we are
        // currently displaying. Entering timeslip list mode saves this in pd,
        // we can't access a row here - there might not be one if it's a new
        // task.
        if (pd->currentMode == FATT_MODE_TIMESLIP_LIST) {
            char *argv[] = {"freeagent", "create-timeslip", pd->relatedTaskId, *input, NULL};
            g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
            // Reload timeslips
            populate_time_slip_entries(sw, pd->relatedTaskId);
        }
        // Just do nothing if on TASK mode
        retv = RELOAD_DIALOG;
    }
    else if ((mretv & MENU_ENTRY_DELETE) == MENU_ENTRY_DELETE)
    {
        // TODO!
        retv = RELOAD_DIALOG;
    }
    return retv;
}

static void fatt_mode_destroy(Mode *sw)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);
    if (pd != NULL)
    {
        g_free(pd);
        mode_set_private_data(sw, NULL);
    }
}

static char *fatt_get_display_value(const Mode *sw, unsigned int selected_line, G_GNUC_UNUSED int *state, G_GNUC_UNUSED GList **attr_list, int get_entry)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);

    // Only return the string if requested, otherwise only set state.
    if (!get_entry)
        return NULL;

    FATTRow *row = g_ptr_array_index(pd->rows, selected_line);
    char* format = "%s";
    if (row->type == FATT_ROW_TIMESLIP_RUNNING) {
        format = "%s\tR";
    }
    return g_strdup_printf(format, row->name);
}

/**
 * @param sw The mode object.
 * @param tokens The tokens to match against.
 * @param index  The index in this plugin to match against.
 *
 * Tokenized match, match tokens to line input.
 *
 * @return TRUE when matches, FALSE otherwise
 */
static int fatt_token_match(const Mode *sw, rofi_int_matcher **tokens, unsigned int index)
{
    FATTModePrivateData *pd = (FATTModePrivateData *)mode_get_private_data(sw);

    // Call default matching function.
    FATTRow *row = g_ptr_array_index(pd->rows, index);
    return helper_token_match(tokens, row->name);
}

Mode mode =
    {
        .abi_version = ABI_VERSION,
        .name = "fatt",
        .cfg_name_key = "fatt",
        ._init = fatt_mode_init,
        ._get_num_entries = fatt_mode_get_num_entries,
        ._result = fatt_mode_result,
        ._destroy = fatt_mode_destroy,
        ._token_match = fatt_token_match,
        ._get_display_value = fatt_get_display_value,
        ._get_message = NULL,
        ._get_completion = NULL,
        ._preprocess_input = NULL,
        .private_data = NULL,
        .free = NULL,
    };