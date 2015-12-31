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

#define LOG_TAG "FingerPrintFpdClient"

#include <cutils/log.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include "fpd_client.h"

#ifndef SOCKET_PATH
#define SOCKET_PATH "/dev/socket/fpd"
#endif

#define FP_BUFFER_SIZE 1024
#define FP_MAX_NAME_LEN 128

enum {
    FP_DETECT_FINGER_DOWN = 0x00260042,
    FP_DETECT_FINGER_UP   = 0x00260043,
    FP_ENROLL             = 0x00260045,
    FP_VERIFY             = 0x00260047,
    FP_CANCEL             = 0x0026004a,
    FP_DEL_TEMPLATE       = 0x0026004b,
    FP_OPEN               = 0x0026004c,
    FP_RELEASE            = 0x0026004d,
    FP_GET_IDS            = 0x0026004e,
};

/* Connects to the socket passed in the 'path' argument.
 */
static int fpd_connect(const char *path) {
    int fd = 0;
    struct sockaddr_un sock;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        ALOGE("socket (%d)", errno);
        // TODO: better error code
        return -ENOENT;
    }

    sock.sun_family = AF_UNIX;
    strcpy(sock.sun_path, path);

    if (connect(fd, (struct sockaddr *) &sock, strlen(sock.sun_path) + sizeof(sock.sun_family)) == -1) {
        perror("connect");
        ALOGE("connect (%d)", errno);
        close(fd);
        return -ENOENT;
    }

    return fd;
}

/* Closes the connection to the passed file descriptor.
 */
static void fpd_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

/* Sends a byte stream contained in send_buffer with size send_buffer_len into
 * the fingerprint app socket. It also reads back a response to the buffer
 * passed in recv_buffer and writes the number of bytes read to recv_buffer_len.
 */
static int fpd_invoke_cmd_locked(unsigned char *send_buffer, uint32_t send_buffer_len,
        unsigned char *recv_buffer, uint32_t *recv_buffer_len) {
    int result  = CMD_RESULT_OK;
    unsigned char recv_local_buf[FP_BUFFER_SIZE];
    int size;

    if (send_buffer_len > FP_BUFFER_SIZE) {
        return CMD_RESULT_FAIL;
    }

    int fd = fpd_connect(SOCKET_PATH);
    if (fd < 0) {
        return RESULT_FAILURE;
    }

    while (send_buffer_len) {
        size = send(fd, send_buffer, send_buffer_len, 0);
        if (size <= 0) {
            result = RESULT_FAILURE;
            goto cmd_end;
        }
        send_buffer_len -= size;
        send_buffer += size;
    }

    result = recv(fd, recv_local_buf, FP_BUFFER_SIZE, 0);
    if (result <= 0) {
        result = RESULT_FAILURE;
        goto cmd_end;
    }
    *recv_buffer_len = result;
    memcpy(recv_buffer, recv_local_buf, result);
    result = CMD_RESULT_OK;

cmd_end:
    fpd_close(fd);
    return result;
}

/* Helper function for sending a generic control command that takes an int
 * and returns an int.
 */
static int fpd_generic_int_cmd(int cmd) {
    int result;
    int cmd_id = cmd;
    uint32_t recv_buf_len;
    int cmd_result;

    result = fpd_invoke_cmd_locked((unsigned char*) &cmd_id, sizeof(cmd_id),
            (unsigned char *) &cmd_result, &recv_buf_len);

    if (result != CMD_RESULT_OK || recv_buf_len != sizeof(int)) {
        return CMD_RESULT_FAIL;
    }

    return cmd_result;
}

int fpd_open() {
    return fpd_generic_int_cmd(FP_OPEN);
}

int fpd_release() {
    return fpd_generic_int_cmd(FP_RELEASE);
}

int fpd_cancel() {
    return fpd_generic_int_cmd(FP_CANCEL);
}

int fpd_detect_finger_down() {
    return fpd_generic_int_cmd(FP_DETECT_FINGER_DOWN);
}

int fpd_detect_finger_up() {
    return fpd_generic_int_cmd(FP_DETECT_FINGER_UP);
}

int fpd_get_enrolled_ids(fpd_enrolled_ids_t *enrolled) {
    int cmd_id = FP_GET_IDS;
    uint32_t recv_len;
    int recv_buffer[MAX_ID_NUM + 2];

    int result = fpd_invoke_cmd_locked((unsigned char *) &cmd_id, sizeof(int),
            (unsigned char *) recv_buffer, &recv_len);

    if (result != CMD_RESULT_OK || recv_len != sizeof(recv_buffer)) {
        return RESULT_FAILURE;
    }

    enrolled->id_num = recv_buffer[0];
    for (int i = 0; i < enrolled->id_num; i++) {
        enrolled->ids[i] = recv_buffer[i+1];
    }

    return recv_buffer[MAX_ID_NUM + 1];
}

int fpd_enroll(char *name, int *id, int *points) {
    int name_len;
    uint32_t recv_len;
    int result[3 + 16];

    name_len = strlen(name);
    if (name_len >= FP_MAX_NAME_LEN || name_len == 0) {
        return RESULT_FAILURE;
    }

    if (points == NULL) {
        return RESULT_FAILURE;
    }

    char cmd[FP_MAX_NAME_LEN + sizeof(int)] = {0};

    *(int*) cmd = FP_ENROLL;
    strcpy(cmd + sizeof(int), name);

    int ret = fpd_invoke_cmd_locked((unsigned char *)cmd, sizeof(cmd),
            (unsigned char *) result, &recv_len);

    if (result[18] == CMD_RESULT_OK && recv_len == sizeof(result)) {
        memcpy(points, result + 2, 16*sizeof(int));
        if (result[0] == 100) {
            *id = result[1];
        }
        return result[0];
    } else if (recv_len != sizeof(result)) {
        return RESULT_FAILURE;
    }

    return result[18];
}

int fpd_verify(int id, int *matched_id) {
    int cmd[3];
    int result[2];
    uint32_t recv_len;

    cmd[0] = FP_VERIFY;
    cmd[1] = id;
    cmd[2] = 0;

    int ret = fpd_invoke_cmd_locked((unsigned char *)cmd, sizeof(cmd),
            (unsigned char *) result, &recv_len);
    if (ret != CMD_RESULT_OK || recv_len != sizeof(result)) {
        return RESULT_FAILURE;
    }

    *matched_id = result[0];
    return result[1];
}

int fpd_verify_all(int *matched_id) {
    return fpd_verify(0, matched_id);
}

int fpd_del_template(int id) {
    int cmd[2];
    int result = CMD_RESULT_OK;
    uint32_t recv_len;

    cmd[0] = FP_DEL_TEMPLATE;
    cmd[1] = id;

    int ret = fpd_invoke_cmd_locked((unsigned char *)cmd, sizeof(cmd),
            (unsigned char *) &result, &recv_len);
    if (ret != CMD_RESULT_OK) {
        result = CMD_RESULT_FAIL;
    }

    return result;
}
