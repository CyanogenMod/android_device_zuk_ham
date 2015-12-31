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
#define LOG_TAG "FingerprintHal"

#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/fingerprint.h>

#include "fpd_sm.h"
#include "fpd_client.h"

static fpd_sm_t *g_fpd_sm;

static int fingerprint_close(hw_device_t *dev)
{
    if (dev) {
        if (g_fpd_sm != NULL) {
            fpd_sm_destroy(g_fpd_sm);
            g_fpd_sm = NULL;
        }
        free(dev);
        return 0;
    } else {
        return -1;
    }
}

static inline int fpd_result_to_hal(int result) {
    return result == FPD_SM_OK ? 0 : FINGERPRINT_ERROR;
}

static int fingerprint_authenticate(struct fingerprint_device __unused *dev) {
    ALOGI("REI Auth start");
    return fpd_result_to_hal(fpd_sm_start_authenticating(g_fpd_sm));
}

static int fingerprint_enroll(struct fingerprint_device __unused *dev,
                                uint32_t timeout_sec) {
    return fpd_result_to_hal(fpd_sm_start_enrolling(g_fpd_sm, timeout_sec));
}

static int fingerprint_cancel(struct fingerprint_device __unused *dev) {
    fpd_sm_cancel_authentication(g_fpd_sm);
    fpd_sm_cancel_enrollment(g_fpd_sm);
    return 0;
}

static int fingerprint_remove(struct fingerprint_device __unused *dev,
                                uint32_t fingerprint_id) {
    return fpd_result_to_hal(fpd_sm_remove_id(g_fpd_sm, fingerprint_id));
}

static int set_notify_callback(struct fingerprint_device *dev,
                                fingerprint_notify_t notify) {
    dev->notify = notify;
    fpd_sm_set_notify(g_fpd_sm, notify);
    return 0;
}

static int fingerprint_get_enrollment_info(struct fingerprint_device __unused *dev,
                                enrollment_info_t __unused **enrollmentInfo) {
    fpd_enrolled_ids_t enrolled;
    if (fpd_sm_get_enrolled_ids(g_fpd_sm, &enrolled) != FPD_SM_OK) {
        return FINGERPRINT_ERROR;
    }

    enrollment_info_t *info = (enrollment_info_t *) malloc(sizeof(enrollment_info_t));
    if (info == NULL) {
        return FINGERPRINT_ERROR;
    }

    info->num_fingers = enrolled.id_num;
    info->fpinfo = NULL;

    if (enrolled.id_num > 0) {
      info->fpinfo = (fingerprint_t *)malloc(sizeof(fingerprint_t) * enrolled.id_num);
      if (info->fpinfo == NULL) {
          free(info);
          return FINGERPRINT_ERROR;
      }
      for (int i = 0; i < enrolled.id_num; i++) {
        info->fpinfo[i].index = enrolled.ids[i];
      }
    }
    *enrollmentInfo = info;

    return 0;
}

static int fingerprint_release_enrollment_info(struct fingerprint_device __unused  *dev,
                                enrollment_info_t *enrollmentInfo) {
    if (enrollmentInfo != NULL) {
        if (enrollmentInfo->fpinfo != NULL) {
            free(enrollmentInfo->fpinfo);
        }
        free(enrollmentInfo);
    }
    return 0;
}

static int fingerprint_get_num_enrollment_steps(struct fingerprint_device __unused *dev) {
      return MAX_ENROLLMENT_STEPS;
}

static int fingerprint_open(const hw_module_t* module, const char __unused *id,
                            hw_device_t** device)
{
    if (device == NULL) {
        ALOGE("NULL device on open");
        return -EINVAL;
    }

    fingerprint_device_t *dev = malloc(sizeof(fingerprint_device_t));
    if (dev == NULL) {
        return -ENOMEM;
    }
    memset(dev, 0, sizeof(fingerprint_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = HARDWARE_MODULE_API_VERSION(1, 1);
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = fingerprint_close;

    dev->authenticate = fingerprint_authenticate;
    dev->cancel = fingerprint_cancel;
    dev->enroll = fingerprint_enroll;
    dev->remove = fingerprint_remove;
    dev->set_notify = set_notify_callback;
    dev->notify = NULL;
    dev->get_enrollment_info = fingerprint_get_enrollment_info;
    dev->release_enrollment_info = fingerprint_release_enrollment_info;
    dev->get_num_enrollment_steps = fingerprint_get_num_enrollment_steps;

    if ((g_fpd_sm = fpd_sm_init()) == NULL) {
        free(dev);
        return -ENODEV;
    }

    *device = (hw_device_t*) dev;
    return 0;
}

static struct hw_module_methods_t fingerprint_module_methods = {
    .open = fingerprint_open,
};

fingerprint_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = FINGERPRINT_MODULE_API_VERSION_1_1,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = FINGERPRINT_HARDWARE_MODULE_ID,
        .name               = "Ham Fingerprint HAL",
        .author             = "Cyanogen, Inc",
        .methods            = &fingerprint_module_methods,
    },
};