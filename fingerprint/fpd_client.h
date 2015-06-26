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
#ifndef _FPD_CLIENT_H
#define _FPD_CLIENT_H

/* This is a low-level API for interfacing with the fingerprint client.
 * This is not thread-safe and will need proper state management to be
 * useful for any kind of complex operation such as enrollment or
 * authorization.
 */

#define MAX_ID_NUM 10

enum {
    CMD_RESULT_OK = 0,
    CMD_RESULT_FAIL = 1,
    RESULT_SUCCESS  = 100,
    RESULT_FAILURE  = 101,
};

typedef struct fpd_enrolled_ids {
    int id_num;
    int ids[MAX_ID_NUM];
} fpd_enrolled_ids_t;

/* Opens the fingerprint TZ application. 'open' is used loosely here, in
 * fact it tells the fingerprint client to load the TZ app. This must be
 * called prior to any other functions.
 */
int fpd_open();

/* Closes the fingerprint TZ application.
 */
int fpd_release();

/* Cancels an ongoing enrollment or authentication operation.
 */
int fpd_cancel();

/* Detects whether a finger is pressed against the sensor.
 * Returns CMD_RESULT_OK when a finger is down, something else when it's 
 * not. Please note that this does not block until a finger is down.
 */
int fpd_detect_finger_down();

/* Detects whether a finger is not pressed against the sensor.
 * Returns CMD_RESULT_OK when a finger is not detected, something else
 * when it is. Please note that this does not block until a finger is up.
 */
int fpd_detect_finger_up();

/* Finds the list of enrolled fingerprint ids and stores it on the
 * structure passed as the argument 'enrolled'.
 * Returns CMD_RESULT_OK when fetching is successful.
 */
int fpd_get_enrolled_ids(fpd_enrolled_ids_t *enrolled);

/* Enrolls a fingerprint. Enrolling is a multi-stage process composed of
 * multiple calls to this function. Callees wanting to enroll should
 * call this function until the returning value is RESULT_SUCCESS. Until
 * then, a percentage of completeness is returned. If enrollment fails,
 * RESULT_FAILURE is returned. Upon returning successfully, the fingerprint
 * id will be stored in the 'id' argument.
 */
int fpd_enroll(char *name, int *id, int *points);

/* Checks whether a fingerprint matches against all the templates stored
 * in the database. Returns CMD_RESULT_OK if a finger matches, something
 * else otherwise. If the command returns successfully, matched_id will
 * contain the fingerprint that was matched.
 */
int fpd_verify_all(int *matched_id);

/* Checks whether a fingerprint matches against the template referenced
 * by 'id'. Returns CMD_RESULT_OK if a finger matches, something else 
 * otherwise. In case of success the id is duplicated into matched_id.
 */
int fpd_verify(int id, int *matched_id);

/* Deletes the fingerprint with the given id from the tempalte database.
 * Returns CMD_RESULT_OK if it was deleted successfully.
 */
int fpd_del_template(int id);

#endif
