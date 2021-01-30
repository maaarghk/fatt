/**
 * external_process.c
 *
 * Helper to call out to an external process and receive stdout in a callback
 *
 * Usage:
 *
 *     GError **error;
 *     char *argv[] = {"git", "show-ref", NULL};
 *     ExternalProcess *externalProcess = external_process_init();
 *     external_process_launch(externalProcess, &error, argv, (ExternalProcessCallback)my_callback_function, *userData);
 *
 * where:
 *
 *     void my_callback_function(GString *buffer, gpointer *userData)
 *     {
 *         g_printf_string(buffer, "stdout was: %s");
 *     }
 */
#include <gmodule.h>

#include "external_process.h"

#define BUFSIZE 64

struct ExternalProcess {
    gchar **argv;

    ExternalProcessCallback stdoutCallback;
    gpointer stdoutCallbackUserData;
    ExternalProcessCallback stderrCallback;
    gpointer stderrCallbackUserData;

    GPid pid;
    guint childWatchId;           /* g_child_watch_add (PID) */

    GIOChannel *stdout;
    guint stdoutWatchId;          /* g_io_add_watch (stdout) */

    GIOChannel *stderr;
    guint stderrWatchId;          /* g_io_add_watch (stderr) */

    GString *stdoutBuffer;
    GString *stderrBuffer;
};


void external_process_free(ExternalProcess *externalProcess);
static gboolean stdout_watch_cb(GIOChannel *source, GIOCondition condition, ExternalProcess *externalProcess);
static gboolean stderr_watch_cb(GIOChannel *source, GIOCondition condition, ExternalProcess *externalProcess);

/**
 * Is called when the spawned child returns. Responsible for collecting data and
 * passing it to the callback
 */
static void child_watch_cb(GPid pid, gint status, ExternalProcess *externalProcess)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) >= 255) {
            g_warning("external_process: Child exited unexpectedly");
            // Check for stderr callback
        }
        if (WEXITSTATUS(status) == 0) {
            externalProcess->stdoutCallback(
                externalProcess->stdoutBuffer,
                externalProcess->stdoutCallbackUserData
            );
        }
    }

    external_process_free(externalProcess);
}

/**
 * Spawns a cli process. Should be passed an externalProcess object with argv
 * set and optionally callback functions that will receive stdout and stderr
 * after the process exits
 */
gboolean external_process_launch(ExternalProcess *externalProcess, GError **error, char **argv, ExternalProcessCallback cb, gpointer userdata)
{
    gint my_stdout, my_stderr;
    gboolean ret;

    externalProcess->stdoutCallback = cb;
    externalProcess->stdoutCallbackUserData = userdata;

    ret = g_spawn_async_with_pipes(
        NULL,                                            // Working directory
        argv,                                            // Argument vector
        NULL,                                            // Environment
        G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, // Flags
        NULL,                                            // Child setup
        NULL,                                            // Data to child setup
        &externalProcess->pid,                           // PID
        NULL,                                            // Stdin
        &my_stdout,                                      // Stdout
        &my_stderr,                                      // Stderr
        error                                            // GError
    );

    if (!ret) {
        // An error occured, do cleanup.
        g_warning("external_process: error calling g_spawn_async_with_pipes");
        external_process_free(externalProcess);
        return FALSE;
    }

    // Open IO channels
    externalProcess->stdout = g_io_channel_unix_new(my_stdout);
    externalProcess->stderr = g_io_channel_unix_new(my_stderr);

    // Set raw encoding and nonblocking mode
    if (
        g_io_channel_set_encoding(externalProcess->stdout, NULL, error) != G_IO_STATUS_NORMAL ||
        g_io_channel_set_encoding(externalProcess->stderr, NULL, error) != G_IO_STATUS_NORMAL ||
        g_io_channel_set_flags(externalProcess->stdout, G_IO_FLAG_NONBLOCK, error) != G_IO_STATUS_NORMAL ||
        g_io_channel_set_flags(externalProcess->stderr, G_IO_FLAG_NONBLOCK, error) != G_IO_STATUS_NORMAL
    ) {
        // An error occured, do cleanup.
        g_warning("external_process: error setting io channel flags");
        external_process_free(externalProcess);
        return FALSE;
    }

    // Turn off buffering
    g_io_channel_set_buffered(externalProcess->stdout, FALSE);
    g_io_channel_set_buffered(externalProcess->stderr, FALSE);

    // Configure IO channel watchers
    externalProcess->stdoutWatchId = g_io_add_watch(
        externalProcess->stdout,        // GIOChannel
        G_IO_IN | G_IO_PRI,             // GIOCondition
        (GIOFunc) stdout_watch_cb,      // GIOFunc callback
        externalProcess                 // callback data
    );
    externalProcess->stderrWatchId = g_io_add_watch(
        externalProcess->stderr,        // GIOChannel
        G_IO_IN | G_IO_PRI,             // GIOCondition
        (GIOFunc) stdout_watch_cb,      // GIOFunc callback
        externalProcess                 // callback data
    );

    // Add child watcher
    externalProcess->childWatchId = g_child_watch_add(
        externalProcess->pid,             // GPid
        (GChildWatchFunc) child_watch_cb, // GChildWAtchFunc callback
        externalProcess                   // callback data
    );

    return TRUE;
}

/**
 * Clean up resources - like launch but in reverse
 */
void external_process_free(ExternalProcess *externalProcess)
{
    GError *error = NULL;

    // Remove child watcher
    if (externalProcess->childWatchId != 0) {
        g_source_remove (externalProcess->childWatchId);
        externalProcess->childWatchId = 0;
    }

    // Close IO channels (internal file descriptors are automatically closed)
    if (externalProcess->stdout != NULL) {
        if (g_io_channel_shutdown(externalProcess->stdout, TRUE, &error) != G_IO_STATUS_NORMAL) {
            g_warning("external_process: Could not shutdown stdout IO channel: %s", error->message);
            g_error_free(error);
            error = NULL;
        }

        g_io_channel_unref(externalProcess->stdout);
        externalProcess->stdout = NULL;
    }
    if (externalProcess->stderr != NULL) {
        if (g_io_channel_shutdown(externalProcess->stderr, TRUE, &error) != G_IO_STATUS_NORMAL) {
            g_warning("external_process: Could not shutdown stderr IO channel: %s", error->message);
            g_error_free(error);
            error = NULL;
        }

        g_io_channel_unref(externalProcess->stderr);
        externalProcess->stderr = NULL;
    }

    // Remove IO watchers
    if (externalProcess->stdoutWatchId != 0) {
        g_source_remove (externalProcess->stdoutWatchId);
        externalProcess->stdoutWatchId = 0;
    }
    if (externalProcess->stderrWatchId != 0) {
        g_source_remove (externalProcess->stderrWatchId);
        externalProcess->stderrWatchId = 0;
    }

    // Close PID
    if (externalProcess->pid != -1) {
        g_spawn_close_pid(externalProcess->pid);
        externalProcess->pid = -1;
    }

    // Remove output buffers
    externalProcess->stdoutBuffer = NULL;
    externalProcess->stderrBuffer = NULL;
}

/**
 * IO watcher for stdout
 */
static gboolean stdout_watch_cb(GIOChannel *source, GIOCondition condition, ExternalProcess *externalProcess)
{
    gchar           buf[BUFSIZE];           /* Temporary buffer */
    gsize           bytes_read;
    GError          *gio_error = NULL;      /* Error returned by functions */
    GError          *error = NULL;          /* Error sent to callbacks */

    // Initialise buffer
    if (externalProcess->stdoutBuffer == NULL) {
        externalProcess->stdoutBuffer = g_string_new("");
    }

    if (g_io_channel_read_chars(source, buf, BUFSIZE, &bytes_read, &gio_error) != G_IO_STATUS_NORMAL) {
        g_warning("external_process: read error on stdout IO Channel: %s", gio_error->message);
        g_error_free(gio_error);
        return TRUE;
    }

    externalProcess->stdoutBuffer = g_string_append_len(externalProcess->stdoutBuffer, buf, bytes_read);

    // Continue calling us
    return TRUE;
}

/**
 * IO watcher for stderr
 */
static gboolean stderr_watch_cb(GIOChannel *source, GIOCondition condition, ExternalProcess *externalProcess)
{
    gchar           buf[BUFSIZE];           /* Temporary buffer */
    gsize           bytes_read;
    GError          *gio_error = NULL;      /* Error returned by functions */
    GError          *error = NULL;          /* Error sent to callbacks */

    // Initialise buffer
    if (externalProcess->stderrBuffer == NULL) {
        externalProcess->stderrBuffer = g_string_new("");
    }

    if (g_io_channel_read_chars(source, buf, BUFSIZE, &bytes_read, &gio_error) != G_IO_STATUS_NORMAL) {
        g_warning("external_process: read error on stderr IO Channel: %s", gio_error->message);
        g_error_free(gio_error);
        return TRUE;
    }

    externalProcess->stderrBuffer = g_string_append_len(externalProcess->stderrBuffer, buf, bytes_read);

    // Continue calling us
    return TRUE;
}

/**
 * Create a blank external process object
 */
ExternalProcess *external_process_init(void)
{
    ExternalProcess *externalProcess = g_new0(ExternalProcess, 1);

    // Initialize PID. -1 means the process is not running
    externalProcess->pid = -1;

    // Initialise IO channels
    externalProcess->stdout = NULL;
    externalProcess->stderr = NULL;

    // Initialise watchers
    externalProcess->childWatchId = 0;
    externalProcess->stdoutWatchId = 0;
    externalProcess->stderrWatchId = 0;

    // Initialise output buffers
    externalProcess->stdoutBuffer = NULL;
    externalProcess->stderrBuffer = NULL;

    return externalProcess;
}
