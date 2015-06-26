/*
 * Copyright (C) 2013-2014, The CyanogenMod Project
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


#include <time.h>
#include <system/audio.h>
#include <platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include <linux/ioctl.h>
#define __force
#define __bitwise
#define __user
#include <sound/asound.h>

#define LOG_TAG "ham-tfa98xx"
#include <log/log.h>

#include <hardware/audio_amplifier.h>

#define QUAT_MI2S_CLK_CTL "QUAT_MI2S Clock"

extern int tfa9890_init(int sRate);
extern int tfa9890_EQset(int mode);
extern int audio_smartpa_enable(int enabled);

typedef struct tfa9890_device {
    amplifier_device_t amp_dev;
    uint32_t current_output_devices;
    int mixer_fd;
    unsigned int quat_mi2s_clk_id;
    pthread_t watch_thread;
} tfa9890_device_t;

static tfa9890_device_t *tfa9890_dev = NULL;

#define SAMPLE_RATE 48000

static void *amp_watch(void *param) {
    struct snd_ctl_event event;
    tfa9890_device_t *tfa9890 = (tfa9890_device_t *) param;
    while(read(tfa9890->mixer_fd, &event, sizeof(struct snd_ctl_event)) > 0) {
        if (event.data.elem.id.numid == tfa9890->quat_mi2s_clk_id) {
            struct snd_ctl_elem_value ev;
            ev.id.numid = tfa9890->quat_mi2s_clk_id;
            if (ioctl(tfa9890->mixer_fd, SNDRV_CTL_IOCTL_ELEM_READ, &ev) < 0)
                continue;
            ALOGD("Got %s event = %d!", QUAT_MI2S_CLK_CTL, ev.value.enumerated.item[0]);
            if (ev.value.enumerated.item[0]) {
                tfa9890_EQset(0);
                audio_smartpa_enable(1);
            } else {
                audio_smartpa_enable(0);
            }
        }
    }
    return NULL;
}

static int is_speaker(uint32_t snd_device) {
    int speaker = 0;

    switch (snd_device) {
        case SND_DEVICE_OUT_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_REVERSE:
        case SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES:
        case SND_DEVICE_OUT_VOICE_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_AND_HDMI:
        case SND_DEVICE_OUT_SPEAKER_AND_USB_HEADSET:
        case SND_DEVICE_OUT_SPEAKER_AND_ANC_HEADSET:
            speaker = 1;
            break;
    }

    return speaker;
}

static int is_voice_speaker(uint32_t snd_device) {
    return snd_device == SND_DEVICE_OUT_VOICE_SPEAKER;
}

static int amp_set_output_devices(amplifier_device_t *device, uint32_t devices) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;

    if (is_speaker(devices)) {
        if (is_voice_speaker(devices)) {
            tfa9890_EQset(1);
        } else {
            tfa9890_EQset(0);
        }
    } else {
        tfa9890_EQset(0);
    }

    return 0;
}

static int amp_dev_close(hw_device_t *device) {
    tfa9890_device_t *tfa9890 = (tfa9890_device_t*) device;

    if (tfa9890) {
        if (tfa9890->mixer_fd >= 0) {
            close(tfa9890->mixer_fd);
        }
        pthread_join(tfa9890->watch_thread, NULL);
        free(tfa9890);
    }

    return 0;
}

static int amp_init(tfa9890_device_t *tfa9890) {
    size_t i;
    int subscribe = 1;
    struct snd_ctl_elem_list elist;
    struct snd_ctl_elem_id *eid = NULL;

    tfa9890_init(SAMPLE_RATE);

    tfa9890->mixer_fd = open("/dev/snd/controlC0", O_RDWR);
    if (tfa9890->mixer_fd < 0) {
        ALOGE("failed to open");
        goto fail;
    }

    memset(&elist, 0, sizeof(elist));
    if (ioctl(tfa9890->mixer_fd, SNDRV_CTL_IOCTL_ELEM_LIST, &elist) < 0) {
        ALOGE("failed to get alsa control list");
        goto fail;
    }

    eid = calloc(elist.count, sizeof(struct snd_ctl_elem_id));
    if (!eid) {
        ALOGE("failed to allocate snd_ctl_elem_id");
        goto fail;
    }

    elist.space = elist.count;
    elist.pids = eid;

    if (ioctl(tfa9890->mixer_fd, SNDRV_CTL_IOCTL_ELEM_LIST, &elist) < 0) {
        ALOGE("failed to get alsa control list");
        goto fail;
    }

    for (i = 0; i < elist.count; i++) {
        struct snd_ctl_elem_info ei;
        ei.id.numid = eid[i].numid;
        if (ioctl(tfa9890->mixer_fd, SNDRV_CTL_IOCTL_ELEM_INFO, &ei) < 0) {
            ALOGE("failed to get alsa control %d info", eid[i].numid);
            goto fail;
        }

        if (!strcmp(QUAT_MI2S_CLK_CTL, (const char *)ei.id.name)) {
            ALOGD("Found %s! %d", QUAT_MI2S_CLK_CTL, ei.id.numid);
            tfa9890->quat_mi2s_clk_id = ei.id.numid;
            break;
        }
    }

    if (i == elist.count) {
        ALOGE("could not find %s", QUAT_MI2S_CLK_CTL);
        goto fail;
    }

    if (ioctl(tfa9890->mixer_fd, SNDRV_CTL_IOCTL_SUBSCRIBE_EVENTS, &subscribe) < 0) {
        ALOGE("failed to subscribe to %s events", QUAT_MI2S_CLK_CTL);
        goto fail;
    }

    pthread_create(&tfa9890->watch_thread, NULL, amp_watch, tfa9890);

    return 0;
fail:
    if (eid)
        free(eid);
    if (tfa9890->mixer_fd >= 0)
        close(tfa9890->mixer_fd);
    return -ENODEV;
}

static int amp_module_open(const hw_module_t *module,
        __attribute__((unused)) const char *name, hw_device_t **device)
{
    if (tfa9890_dev) {
        ALOGE("%s:%d: Unable to open second instance of TFA9890 amplifier\n",
                __func__, __LINE__);
        return -EBUSY;
    }

    tfa9890_dev = calloc(1, sizeof(tfa9890_device_t));
    if (!tfa9890_dev) {
        ALOGE("%s:%d: Unable to allocate memory for amplifier device\n",
                __func__, __LINE__);
        return -ENOMEM;
    }

    tfa9890_dev->amp_dev.common.tag = HARDWARE_DEVICE_TAG;
    tfa9890_dev->amp_dev.common.module = (hw_module_t *) module;
    tfa9890_dev->amp_dev.common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
    tfa9890_dev->amp_dev.common.close = amp_dev_close;

    tfa9890_dev->amp_dev.set_output_devices = amp_set_output_devices;

    tfa9890_dev->current_output_devices = 0;

    if (amp_init(tfa9890_dev)) {
        free(tfa9890_dev);
        return -ENODEV;
    }

    *device = (hw_device_t *) tfa9890_dev;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = amp_module_open,
};

amplifier_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AMPLIFIER_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AMPLIFIER_HARDWARE_MODULE_ID,
        .name = "Ham amplifier HAL",
        .author = "The CyanogenMod Open Source Project",
        .methods = &hal_module_methods,
    },
};
