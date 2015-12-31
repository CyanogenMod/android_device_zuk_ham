#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fpd_client.h"

int main() {
    printf("Interactive fingerprint sanity check\n");
    printf("WARNING: This destroys all templates\n");

    fpd_release();
    if (fpd_open()) {
        printf("Open failed, aborting\n");
    }

    printf("Cleaning up\n");

    fpd_enrolled_ids_t ids;
    int enrolled_ids_result = fpd_get_enrolled_ids(&ids);
    printf("Found %d old ids, removing them.\n", ids.id_num);
    for (int i = 0; i < ids.id_num; i++) {
        fpd_del_template(ids.ids[i]);
    }

    printf("We're now going to enroll 5 of your fingers.\n");

    for (int finger = 0; finger < 1; finger ++) {
        printf("Place one of your ugly fingers in the sensor\n");
        while (fpd_detect_finger_down()) {
            sleep(1);
        }

        printf("Thanks, let me enroll that finger.\n");
        while (1) {
            int points[16];
            int id;
            int ret = fpd_enroll("a", &id, points);

            if (ret == RESULT_FAILURE) {
                printf("Aborting, enroling failed\n");
                return -1;
            }

            if (ret == 100) {
                printf("Enrolled finger %d\n", id);
                break;
            }

            for (int j = 0; j < 16; j++) {
                printf("%d ", points[j]);
            }
            printf("\n");
        }

        printf("Lift that greasy finger out of the sensor\n");
        while (fpd_detect_finger_up()) {
            sleep(1);
        }
    }

    printf("Ok, now try it out with different fingers, CTRL-C when you're happy\n");
    while (1) {
        sleep(1);
        int matched;
        int result = fpd_verify_all(&matched);
        printf("Finger %d was %s\n", matched, result ? "not verified" : "verified");
    }

    fpd_release();
}
