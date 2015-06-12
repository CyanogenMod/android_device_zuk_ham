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

#define LOG_TAG "ham-tfa98xx"
#include <log/log.h>

extern int tfa9890_init(int sRate);
extern int tfa9890_EQset(int mode);
extern int audio_smartpa_enable(int enabled);

#define SAMPLE_RATE 48000

static int is_output_device(int snd_device) {
    return snd_device > SND_DEVICE_OUT_BEGIN && snd_device < SND_DEVICE_OUT_END;
}

static int is_speaker(int snd_device) {
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

int amplifier_open(void) {
    ALOGV("initializing");
    tfa9890_init(SAMPLE_RATE);
    return 0;
}

void amplifier_set_devices(int snd_device) {
    int output = is_output_device(snd_device);
    int speaker = is_speaker(snd_device);

    if (speaker) {
        ALOGV("turning speaker on for speaker device %d", snd_device);
        tfa9890_EQset(0);
        audio_smartpa_enable(1);
    } else if (output) {
        ALOGV("turning speaker off for output device %d", snd_device);
        audio_smartpa_enable(0);
    } else {
        ALOGV("%d not an output, ignoring", snd_device);
    }
}

int amplifier_set_mode(audio_mode_t mode __attribute__((unused))) {
    return 0;
}

int amplifier_close(void) {
    return 0;
}
