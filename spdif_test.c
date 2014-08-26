/*
 * Copyright (C) 2007 The Android Open Source Project
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

#define LOG_TAG "hdmitest"

#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>  // for utimes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>

#include <cutils/log.h>
#include "common.h"
#include "screen_test.h"
#include "alsa_audio.h"
#include "extra-functions.h"
#include "script.h"

#ifdef DEBUG
#define LOG(x...) printf(x)
#else
#define LOG(x...)
#endif

#define AUDIO_HW_OUT_PERIOD_MULT 8 // (8 * 128 = 1024 frames)
#define AUDIO_HW_OUT_PERIOD_CNT 4
#define AUDIO_HW_OUT_SPDIF_FLAG 1

#define AUDIO_CARD_FILE		"/proc/asound/cards"

// ---------------------------------------------------------------------------
static int spdif_dev_check(void)
{
	FILE *fp;
	unsigned char state = 0;
	
	fp = fopen(AUDIO_CARD_FILE, "r");
	if(fp == NULL) {
		LOG("Asound card file is not exist\n");
		return -1;
	}
	int ch;
	while(!feof(fp)){
		ch = fgetc(fp);
		if(ch=='1'){//SDPIF
			state = 1;
			LOG("Card 1 SPDIF exist.\n");
			break;
		}
	}
	fclose(fp);
	return (state);
}

static void spdif_audio_test(char *filename)
{
	struct pcm* pcmOut = NULL;
	FILE *fp = NULL;
	unsigned int bufsize, num_read;
	char *buffer;

	unsigned inFlags = PCM_IN;
	unsigned outFlags = PCM_OUT;
	unsigned flags =  PCM_STEREO;
	
	LOG("[%s] file %s\n", __FUNCTION__, filename);
	
	if(filename == NULL) {
		LOG("[%s] audio filename is NULL\n", __FUNCTION__);
		return;
	}
	
	flags |= (AUDIO_HW_OUT_PERIOD_MULT - 1) << PCM_PERIOD_SZ_SHIFT;
	flags |= (AUDIO_HW_OUT_PERIOD_CNT - PCM_PERIOD_CNT_MIN)<< PCM_PERIOD_CNT_SHIFT;

	inFlags |= flags;
	outFlags |= flags;
	outFlags |= AUDIO_HW_OUT_SPDIF_FLAG;//Use SPDIF OUT
	
	fp = fopen(filename, "rb");
	if(NULL == fp) {
		LOG("[%s] open file %s error\n", __FUNCTION__, filename);
		return;
	}
	fseek(fp, 0, SEEK_SET);

	pcmOut = pcm_open(outFlags);
	if (!pcm_ready(pcmOut)) {
		pcm_close(pcmOut);
		pcmOut = NULL;
		return;
	}

	bufsize = pcm_buffer_size(pcmOut);
	buffer = malloc(bufsize);
    if (!buffer) {
        LOG("Unable to allocate %d bytes\n", bufsize);
        free(buffer);
        pcm_close(pcmOut);
        return;
    }

	do {
        num_read = fread(buffer, 1, bufsize, fp);
        if (num_read > 0) {
            if (pcm_write(pcmOut, buffer, num_read)) {
                LOG("Error playing sample\n");
                break;
            }
        }
    } while (num_read > 0);

    pcm_close(pcmOut);
    free(buffer);
    fclose(fp);
    LOG("[%s] done\n", __FUNCTION__);
}

void* spdif_test(void* argv)
{	
	int spdstate;
	char sound[256];
	int ret;
	
	memset(sound, 0, 256);
	ret = script_fetch("SPDIF", "sound_file", (int *)sound, 256/4);
	
	LOG("[%s] sound file is %s ret is %d\n", __FUNCTION__, sound, ret);
	while(1) {
		sleep(1);
		spdstate = spdif_dev_check();
		LOG("[%s] spdif state is %d\n", __FUNCTION__, spdstate);
		sleep(3);
		if(spdstate) {
			// Test audio
			LOG("[%s] start test audio\n", __FUNCTION__);
			spdif_audio_test(sound);
		}
	}
    return 0;
}
