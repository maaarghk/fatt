/**
 * external_process.h
 *
 */

struct ExternalProcess;
typedef struct ExternalProcess ExternalProcess;

typedef void (*ExternalProcessCallback) (GString *buffer, const gpointer userData);

gboolean external_process_launch(ExternalProcess *externalProcess, GError **error, char **argv, ExternalProcessCallback cb, gpointer userdata);
void external_process_free(ExternalProcess *externalProcess);
ExternalProcess *external_process_init(void);