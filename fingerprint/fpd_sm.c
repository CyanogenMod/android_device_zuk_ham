/*
 * Copyright (C) 2015 Cyanogen, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cutils/log.h>
#include <pthread.h>

#include "fpd_sm.h"
#include "fpd_client.h"

/* The possible states that the state machine might be in.
 */
typedef enum {
  FPD_SM_IDLE,
  FPD_SM_AUTHENTICATING,
  FPD_SM_ENROLLING
} fpd_sm_state_t;

/* States for the authentication/enrollment worker function.
 */
typedef enum {
    FPD_WORKER_OK,
    FPD_WORKER_CANCELED,
} fpd_worker_state_t;

/* The state machine structure contains the current state, a mutex
 * that ensures consistency of the state transitions, the current
 * worker thread and its current state.
 */
struct fpd_sm {
    fpd_sm_state_t state;
    pthread_mutex_t state_mutex;
    pthread_mutexattr_t state_mutex_attrs;
    fingerprint_notify_t notify;
    pthread_t worker;
    fpd_worker_state_t worker_state;
};

/* The argument for worker functions.
 */
typedef struct {
    fpd_sm_t *sm;
    uint32_t timeout_sec;
} fpd_worker_args_t;

fpd_sm_t *fpd_sm_init() {
    fpd_sm_t *sm;

    if (fpd_open() != CMD_RESULT_OK) {
        ALOGE("Unable to start the fingerprint TZ app");
        return NULL;
    }

    sm = (fpd_sm_t *) malloc(sizeof(fpd_sm_t));
    if (sm == NULL) {
        fpd_release();
        return NULL;
    }

    sm->state = FPD_SM_IDLE;
    pthread_mutexattr_init(&sm->state_mutex_attrs);
    pthread_mutexattr_settype(&sm->state_mutex_attrs, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&sm->state_mutex, &sm->state_mutex_attrs);
    sm->notify = NULL;

    return sm;
}

fpd_sm_state_t fpd_sm_get_state(fpd_sm_t *sm) {
    fpd_sm_state_t state;

    pthread_mutex_lock(&sm->state_mutex);
    state = sm->state;
    pthread_mutex_unlock(&sm->state_mutex);

    return state;
}

fpd_sm_state_t fpd_sm_is_idle(fpd_sm_t *sm) {
    return fpd_sm_get_state(sm) == FPD_SM_IDLE;
}

fpd_sm_status_t fpd_sm_destroy(fpd_sm_t *sm) {
    if (!fpd_sm_is_idle(sm)) {
        return FPD_SM_ERR_NOT_IDLE;
    }

    if (sm != NULL) {
        pthread_mutexattr_destroy(&sm->state_mutex_attrs);
        pthread_mutex_destroy(&sm->state_mutex);
        free(sm);
        fpd_release();
    }
    return FPD_SM_OK;
}

void fpd_sm_set_notify(fpd_sm_t *sm, fingerprint_notify_t notify) {
    pthread_mutex_lock(&sm->state_mutex);
    sm->notify = notify;
    pthread_mutex_unlock(&sm->state_mutex);
}

static void fpd_sm_notify(fpd_sm_t *sm, fingerprint_msg_t msg) {
  pthread_mutex_lock(&sm->state_mutex);
  if (sm->notify != NULL) {
    sm->notify(msg);
  }
  pthread_mutex_unlock(&sm->state_mutex);
}

static void fpd_sm_notify_error(fpd_sm_t *sm, fingerprint_error_t error) {
    fingerprint_msg_t msg;
    msg.type = FINGERPRINT_ERROR;
    msg.data.error = error;

    fpd_sm_notify(sm, msg);
}

static void fpd_sm_notify_enrolled(fpd_sm_t *sm, int index, int samples_remaining, uint16_t area) {
    fingerprint_msg_t msg;
    msg.type = FINGERPRINT_TEMPLATE_ENROLLING;
    msg.data.enroll.id = index;
    msg.data.enroll.data_collected_bmp = area;
    msg.data.enroll.samples_remaining = samples_remaining;

    fpd_sm_notify(sm, msg);
}

static void fpd_sm_notify_acquired(fpd_sm_t *sm, fingerprint_acquired_info_t info) {
    fingerprint_msg_t msg;
    msg.type = FINGERPRINT_ACQUIRED;
    msg.data.acquired.acquired_info = info;

    fpd_sm_notify(sm, msg);
}

static void fpd_sm_notify_processed(fpd_sm_t *sm, int index) {
    fingerprint_msg_t msg;
    msg.type = FINGERPRINT_PROCESSED;
    msg.data.processed.id = index;

    fpd_sm_notify(sm, msg);
}

static void fpd_sm_notify_removed(fpd_sm_t *sm, int index) {
    fingerprint_msg_t msg;
    msg.type = FINGERPRINT_TEMPLATE_REMOVED;
    msg.data.removed.id = index;

    fpd_sm_notify(sm, msg);
}

void *authenticate_func(void *arg_auth) {
    fpd_worker_args_t *args = (fpd_worker_args_t*) arg_auth;
    fpd_sm_t *sm = args->sm;

    free(arg_auth);

    for (;;) {
        pthread_mutex_lock(&sm->state_mutex);
        fpd_worker_state_t worker_state = sm->worker_state;
        pthread_mutex_unlock(&sm->state_mutex);

        if (worker_state == FPD_WORKER_CANCELED) {
            fpd_sm_notify_error(sm, FINGERPRINT_ERROR_CANCELED);
            break;
        }

        int finger = 0;
        if (CMD_RESULT_OK == fpd_verify_all(&finger)) {
            fpd_sm_notify_acquired(sm, FINGERPRINT_ACQUIRED_GOOD);
            fpd_sm_notify_processed(sm, finger);
            break;
        }
    }

    pthread_mutex_lock(&sm->state_mutex);
    sm->state = FPD_SM_IDLE;
    pthread_mutex_unlock(&sm->state_mutex);

    return NULL;
}

fpd_sm_status_t fpd_sm_start_authenticating(fpd_sm_t *sm) {
    fpd_sm_status_t result = FPD_SM_OK;

    pthread_mutex_lock(&sm->state_mutex);

    if (sm->state != FPD_SM_IDLE) {
        result = FPD_SM_ERR_NOT_IDLE;
        goto end;
    }

    fpd_worker_args_t *args = (fpd_worker_args_t*) malloc(sizeof(fpd_worker_args_t));
    if (args == NULL) {
        result = FPD_SM_FAILED;
        goto end;
    }

    args->sm = sm;

    sm->state = FPD_SM_AUTHENTICATING;
    sm->worker_state = FPD_WORKER_OK;
    pthread_create(&sm->worker, NULL, authenticate_func, args);

end:
    pthread_mutex_unlock(&sm->state_mutex);
    return result;
}

fpd_sm_status_t fpd_sm_cancel_authentication(fpd_sm_t *sm) {
    int result = FPD_SM_OK;

    pthread_mutex_lock(&sm->state_mutex);

    if (sm->state == FPD_SM_AUTHENTICATING) {
        sm->worker_state = FPD_WORKER_CANCELED;
    } else {
        result = FPD_SM_FAILED;
    }

    pthread_mutex_unlock(&sm->state_mutex);

    return result;
}

static int has_timed_out(struct timespec *when) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  return now.tv_sec > when->tv_sec || (now.tv_sec == when->tv_sec && now.tv_nsec > when->tv_nsec);
}

void *enroll_func(void *arg_enroll) {
    fpd_worker_args_t *args = (fpd_worker_args_t*) arg_enroll;
    fpd_sm_t *sm = args->sm;

    struct timespec timeout;
    clock_gettime(CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec += args->timeout_sec;

    int points[16];

    // We own the args and don't need them anymore
    free(arg_enroll);

    for (;;) {
        pthread_mutex_lock(&sm->state_mutex);
        fpd_worker_state_t worker_state = sm->worker_state;
        pthread_mutex_unlock(&sm->state_mutex);

        if (worker_state == FPD_WORKER_CANCELED) {
            fpd_cancel();
            fpd_sm_notify_error(sm, FINGERPRINT_ERROR_CANCELED);
            break;
        }

        if (has_timed_out(&timeout)) {
            fpd_cancel();
            fpd_sm_notify_error(sm, FINGERPRINT_ERROR_TIMEOUT);
            break;
        }

        // TODO: Figure out what to do with the name,
        if (CMD_RESULT_OK == fpd_detect_finger_down()) {
          int finger = 0;
          int result = fpd_enroll("fp", &finger, points);
          if (result >= 0 && result <= 100) {
            fpd_sm_notify_enrolled(sm, finger, 100 - result, 0);

            if (result == 100) {
              break;
            }

            // This will never be a busy loop because detect_finger_up sleeps for few
            // milliseconds while it polls the finger.
            while (CMD_RESULT_OK != fpd_detect_finger_up() && !has_timed_out(&timeout));
          }
        }
    }

    pthread_mutex_lock(&sm->state_mutex);
    sm->state = FPD_SM_IDLE;
    pthread_mutex_unlock(&sm->state_mutex);

    return NULL;
}


fpd_sm_status_t fpd_sm_start_enrolling(fpd_sm_t *sm, uint32_t timeout_sec) {
    fpd_sm_status_t result = FPD_SM_OK;

    pthread_mutex_lock(&sm->state_mutex);

    if (sm->state != FPD_SM_IDLE) {
        result = FPD_SM_ERR_NOT_IDLE;
        goto end;
    }

    fpd_enrolled_ids_t enrolled;
    if (fpd_get_enrolled_ids(&enrolled) != CMD_RESULT_OK) {
        result = FPD_SM_FAILED;
        fpd_sm_notify_error(sm, FINGERPRINT_ERROR_HW_UNAVAILABLE);
        goto end;
    }

    if (enrolled.id_num > MAX_ID_NUM) {
        result = FPD_SM_FAILED;
        fpd_sm_notify_error(sm, FINGERPRINT_ERROR_NO_SPACE);
        goto end;
    }

    sm->state = FPD_SM_ENROLLING;
    sm->worker_state = FPD_WORKER_OK;

    fpd_worker_args_t *args = (fpd_worker_args_t*) malloc(sizeof(fpd_worker_args_t));
    args->sm = sm;
    args->timeout_sec = timeout_sec;

    pthread_create(&sm->worker, NULL, enroll_func, args);

end:
    pthread_mutex_unlock(&sm->state_mutex);
    return result;
}

fpd_sm_status_t fpd_sm_cancel_enrollment(fpd_sm_t *sm) {
    int result = FPD_SM_OK;

    pthread_mutex_lock(&sm->state_mutex);

    if (sm->state == FPD_SM_ENROLLING) {
        sm->worker_state = FPD_WORKER_CANCELED;
    } else {
        result = FPD_SM_FAILED;
    }

    pthread_mutex_unlock(&sm->state_mutex);

    return result;
}

fpd_sm_status_t fpd_sm_get_enrolled_ids(fpd_sm_t *sm, fpd_enrolled_ids_t *enrolled) {
    int result = FPD_SM_OK;

    pthread_mutex_lock(&sm->state_mutex);
    if (sm->state != FPD_SM_IDLE) {
        result = FPD_SM_ERR_NOT_IDLE;
        goto end;
    }

    if (fpd_get_enrolled_ids(enrolled) != CMD_RESULT_OK) {
        fpd_sm_notify_error(sm, FINGERPRINT_ERROR_HW_UNAVAILABLE);
        result = FPD_SM_FAILED;
        goto end;
    }

end:
    pthread_mutex_unlock(&sm->state_mutex);
    return FPD_SM_OK;
}

fpd_sm_status_t fpd_sm_remove_id_locked(fpd_sm_t *sm, int id) {
    if (fpd_del_template(id) == FPD_SM_OK) {
        fpd_sm_notify_removed(sm, id);
        return FPD_SM_OK;
    }
    return FPD_SM_FAILED;
}

fpd_sm_status_t fpd_sm_remove_id(fpd_sm_t *sm, int id) {
    int result = FPD_SM_OK;

    pthread_mutex_lock(&sm->state_mutex);

    fpd_enrolled_ids_t enrolled;
    if (fpd_get_enrolled_ids(&enrolled) != CMD_RESULT_OK) {
        fpd_sm_notify_error(sm, FINGERPRINT_ERROR_HW_UNAVAILABLE);
        result = FPD_SM_FAILED;
        goto end;
    }

    for (int i = 0; i < enrolled.id_num; i++) {
        if (id == 0 || id == enrolled.ids[i]) {
            if (fpd_sm_remove_id_locked(sm, enrolled.ids[i]) != FPD_SM_OK) {
                result = FPD_SM_FAILED;
                goto end;
            }
        }
    }

end:
    pthread_mutex_unlock(&sm->state_mutex);
    return result;
}
