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

#ifndef _FPD_SM_H
#define _FPD_SM_H

#include <hardware/hardware.h>
#include <hardware/fingerprint.h>

#include "fpd_client.h"

/* Represents a fingerprint state machine, this structure is intentionally
 * opaque, please do not assume anything about its representation, allocation
 * and deallocation are managed by fpd_sm_init/destroy which properly manage
 * the state machine behavior.
 */
typedef struct fpd_sm fpd_sm_t;

/* Return states for state machine calls, they should be self-explanatory.
 */
typedef enum {
  FPD_SM_OK,
  FPD_SM_FAILED,
  FPD_SM_ERR_NOT_IDLE,
} fpd_sm_status_t;

/* Allocates and initializes a fingerprint state machine.
 */
fpd_sm_t *fpd_sm_init();

/* De-initializes and frees a fingerprint state machine passed as the first
 * argument. It returns FPD_SM_OK when successful.
 * Please note that this can fail if the state machine is not idle, in that
 * case it will return FPD_SM_ERR_NOT_IDLE. Pending authentications or
 * enrollments should be cancelled prior to destroying.
 */
fpd_sm_status_t fpd_sm_destroy(fpd_sm_t *sm);

/* Sets the notification function for asynchronous events. When set, it will
 * receive events throughout the lifecycle of the state machine, please refer
 * to the fingerprint hal documentation for the available notifications.
 */
void fpd_sm_set_notify(fpd_sm_t *sm, fingerprint_notify_t notify);

/* Switches the state machine to an authenticating state. If the state machine
 * is not idle, it immediately returns with FPD_SM_ERR_NOT_IDLE.
 *
 * Otherwise, it will switch the state machine to an authenticating state and
 * stays there until:
 * a) A fingerprint is authenticated.
 * b) The action is explicitly cancelled by fpd_sm_cancel_authentication.
 *
 * If the state transition is successful it will return FPD_SM_OK.
 */
fpd_sm_status_t fpd_sm_start_authenticating(fpd_sm_t *sm);

/* Cancels an ongoing authentication state. If the state machine is not in an
 * authenticating state, this will immediately return with FPD_SM_FAILED.
 *
 * Otherwise, it will start the cancellation of the task and return FPD_SM_OK.
 * Please note that the authentication process is not cancelled immediately
 * but shortly after.
 */
fpd_sm_status_t fpd_sm_cancel_authentication(fpd_sm_t *);

/* Switches the state machine to an enrolling state. If the state machine
 * is not idle, it immediately returns with FPD_SM_ERR_NOT_IDLE.
 *
 * Otherwise, it will switch the state machine to an enrollment state and
 * stays there until:
 * a) A fingerprint is enrolled.
 * b) The timeout in timeout_sec is reached.
 * c) The action is explicitly cancelled by fpd_sm_cancel_enrollment.
 *
 * If the state transition is successful it will return FPD_SM_OK.
 */
fpd_sm_status_t fpd_sm_start_enrolling(fpd_sm_t *sm, uint32_t timeout_sec);

/* Cancels an ongoing enrollment state. If the state machine is not in an
 * authenticating state, this will immediately return with FPD_SM_FAILED.
 *
 * Otherwise, it will start the cancellation of the task and return FPD_SM_OK.
 * Please note that the authentication process is not cancelled immediately
 * but shortly after.
 */
fpd_sm_status_t fpd_sm_cancel_enrollment(fpd_sm_t *sm);

/* Obtains the enrolled fingerprints, these are stored in the structure passed
 * on the enroll argument.
 * If the state machine is not in an authenticating state, this will immediately
 * return with FPD_SM_FAILED.
 *
 * Otherwise, it will return FPD_SM_OK.
 */
fpd_sm_status_t fpd_sm_get_enrolled_ids(fpd_sm_t *sm, fpd_enrolled_ids_t *enrolled);

/* Removes the enrolled fingerprint matching the id passed as argument. If the id
 * is 0, all enrolled fingerprints are removed.
 */
fpd_sm_status_t fpd_sm_remove_id(fpd_sm_t *sm, int id);

#endif
