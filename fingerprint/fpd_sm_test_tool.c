#include <stdio.h>
#include "fpd_sm.h"

#include <readline/readline.h>
#include <readline/history.h>

// We're linking with bash dependencies for readline and
// this is weirdly not exported.
void xfree(char *string)
{
  if (string)
    free (string);
}

void notification_error(fingerprint_error_t error) {
    printf("[ERROR] ");
    switch (error) {
        case FINGERPRINT_ERROR_HW_UNAVAILABLE:
            printf("hardware not available");
            break;
        case FINGERPRINT_ERROR_UNABLE_TO_PROCESS:
            printf("unable to process");
            break;
        case FINGERPRINT_ERROR_TIMEOUT:
            printf("operation timed out");
            break;
        case FINGERPRINT_ERROR_NO_SPACE:
            printf("no space available");
            break;
        case FINGERPRINT_ERROR_CANCELED:
            printf("operation canceled");
            break;
        case FINGERPRINT_ERROR_UNABLE_TO_REMOVE:
            printf("unable to remove");
            break;
    }
    printf("\n");
}

void notification_acquired(fingerprint_acquired_t acquired) {
    printf("[ACQUIRED] ");
    switch (acquired.acquired_info) {
        case FINGERPRINT_ACQUIRED_GOOD:
            printf("good");
            break;
        case FINGERPRINT_ACQUIRED_PARTIAL:
            printf("partial");
            break;
        case FINGERPRINT_ACQUIRED_INSUFFICIENT:
            printf("insufficient");
            break;
        case FINGERPRINT_ACQUIRED_IMAGER_DIRTY:
            printf("image dirty");
            break;
        case FINGERPRINT_ACQUIRED_TOO_SLOW:
            printf("too slow");
            break;
        case FINGERPRINT_ACQUIRED_TOO_FAST:
            printf("too fast");
            break;
    }
    printf("\n");
}

void notification_processed(fingerprint_processed_t processed) {
    printf("[PROCESSED] finger %u\n", processed.id);
}

void notification_enroll(fingerprint_enroll_t enrolled) {
    printf("[ENROLLED] finger %u area %d still need %d samples\n", enrolled.id, enrolled.data_collected_bmp, enrolled.samples_remaining);
}

void notification_template_removed(fingerprint_removed_t removed) {
    printf("[REMOVED] finger %u\n", removed.id);
}

void notification_printer(fingerprint_msg_t msg) {
    printf("\n");
    switch (msg.type) {
        case FINGERPRINT_ERROR:
            notification_error(msg.data.error);
            break;
        case FINGERPRINT_ACQUIRED:
            notification_acquired(msg.data.acquired);
            break;
        case FINGERPRINT_PROCESSED:
            notification_processed(msg.data.processed);
            break;
        case FINGERPRINT_TEMPLATE_ENROLLING:
            notification_enroll(msg.data.enroll);
            break;
        case FINGERPRINT_TEMPLATE_REMOVED:
            notification_template_removed(msg.data.removed);
            break;
    }
}

void show_help() {
  printf("> Commands:\n");
  printf("\thelp: Show this message\n");
  printf("\tlist: List enrolled fingerprints\n");
  printf("\tauth: Start authentication\n");
  printf("\tcancelauth: Cancels an ongoing authorization session\n");
  printf("\tenroll [timeout]: Start enrollment, optionally define a timeout (default: 60secs)\n");
  printf("\tcancelenroll: Cancels an ongoing enrollment session\n");
  printf("\tremove [id]: Remove a fingerprint, optionally define an id, empty means delete all\n");
  printf("\texit\n");
}

void repl(fpd_sm_t *sm) {
    char *line;
    while ((line = readline("fp> ")) != NULL) {
      if (!strncmp("help", line, 4)) {
        show_help();
      } else if (!strncmp("auth", line, 4)) {
        fpd_sm_start_authenticating(sm);
      } else if (!strncmp("enroll", line, 4)) {
        int timeout = 60;
        sscanf(line+6, "%d", &timeout);
        fpd_sm_start_enrolling(sm, timeout);
      } else if (!strncmp("cancelauth", line, 10)) {
        fpd_sm_cancel_authentication(sm);
      } else if (!strncmp("cancelenroll", line, 12)) {
        fpd_sm_cancel_enrollment(sm);
      }else if (!strncmp("list", line, 4)) {
        fpd_enrolled_ids_t ids;

        fpd_sm_get_enrolled_ids(sm, &ids);
        printf("Found %d ids:\n", ids.id_num);
        for (int i = 0; i < ids.id_num; i++) {
          printf("\t%d\n", ids.ids[i]);
        }
      } else if (!strncmp("remove", line, 6)) {
        int finger = 0;
        sscanf(line+6, "%d", &finger);
        fpd_sm_remove_id(sm, finger);
      } else if (!strncmp("exit", line, 4)) {
        return;
      }
    }
}

int main() {
    printf("Starting...\n");

    fpd_sm_t *sm = fpd_sm_init();
    fpd_sm_set_notify(sm, notification_printer);

    repl(sm);

    fpd_sm_destroy(sm);
    return 0;
}
