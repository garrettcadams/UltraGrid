/**
 * @file   video_display/pipe.cpp
 * @author Martin Pulec     <pulec@cesnet.cz>
 *
 * @brief  This is an umbrella header for video functions.
 */
/*
 * Copyright (c) 2014 CESNET, z. s. p. o.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of CESNET nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "video.h"
#include "video_display.h"
#include "video_display/pipe.h"

#include "hd-rum-translator/hd-rum-decompress.h"

struct state_pipe {
        struct module *parent;
        frame_recv_delegate *delegate;
        struct video_desc desc;
};

static struct display *display_pipe_fork(void *state)
{
        struct state_pipe *s = (struct state_pipe *) state;
        char fmt[2 + sizeof(void *) * 2 + 1] = "";
        struct display *out;

        snprintf(fmt, sizeof fmt, "%p", s->delegate);
        int rc = initialize_video_display(s->parent,
                "pipe", fmt, 0, &out);
        if (rc == 0) return out; else return NULL;
}

void *display_pipe_init(struct module *parent, const char *fmt, unsigned int flags)
{
        UNUSED(flags);
        frame_recv_delegate *delegate;

        if (!fmt || strlen(fmt) == 0 || strcmp(fmt, "help") == 0) {
                fprintf(stderr, "Pipe dummy video driver. For internal usage - please do not use.\n");
                return nullptr;
        }

        sscanf(fmt, "%p", &delegate);

        struct state_pipe *s = new state_pipe{parent, delegate, video_desc()};

        return s;
}

void display_pipe_done(void *state)
{
        struct state_pipe *s = (struct state_pipe *)state;
        delete s;
}

struct video_frame *display_pipe_getf(void *state)
{
        struct state_pipe *s = (struct state_pipe *)state;

        return vf_alloc_desc_data(s->desc);
}

int display_pipe_putf(void *state, struct video_frame *frame, int flags)
{
        struct state_pipe *s = (struct state_pipe *) state;

        if (flags != PUTF_DISCARD) {
                s->delegate->frame_arrived(frame);
        }

        return TRUE;
}

display_type_t *display_pipe_probe(void)
{
        display_type_t *dt;

        dt = (display_type_t *) calloc(1, sizeof(display_type_t));
        if (dt != NULL) {
                dt->id = DISPLAY_PIPE_ID;
                dt->name = "pipe";
                dt->description = "No display device";
        }
        return dt;
}

void display_pipe_run(void *state)
{
        UNUSED(state);
}

int display_pipe_get_property(void *state, int property, void *val, size_t *len)
{
        UNUSED(state);
        codec_t codecs[] = {UYVY};
        enum interlacing_t supported_il_modes[] = {PROGRESSIVE, INTERLACED_MERGED, SEGMENTED_FRAME};
        int rgb_shift[] = {0, 8, 16};

        switch (property) {
                case DISPLAY_PROPERTY_CODECS:
                        if(sizeof(codecs) <= *len) {
                                memcpy(val, codecs, sizeof(codecs));
                        } else {
                                return FALSE;
                        }

                        *len = sizeof(codecs);
                        break;
                case DISPLAY_PROPERTY_RGB_SHIFT:
                        if(sizeof(rgb_shift) > *len) {
                                return FALSE;
                        }
                        memcpy(val, rgb_shift, sizeof(rgb_shift));
                        *len = sizeof(rgb_shift);
                        break;
                case DISPLAY_PROPERTY_BUF_PITCH:
                        *(int *) val = PITCH_DEFAULT;
                        *len = sizeof(int);
                        break;
                case DISPLAY_PROPERTY_SUPPORTED_IL_MODES:
                        if(sizeof(supported_il_modes) <= *len) {
                                memcpy(val, supported_il_modes, sizeof(supported_il_modes));
                        } else {
                                return FALSE;
                        }
                        *len = sizeof(supported_il_modes);
                        break;
                case DISPLAY_PROPERTY_VIDEO_MODE:
                        *(int *) val = DISPLAY_PROPERTY_VIDEO_SEPARATE_TILES;
                        *len = sizeof(int);
                        break;
                case DISPLAY_PROPERTY_SUPPORTS_MULTI_SOURCES:
                        ((struct multi_sources_supp_info *) val)->val = true;
                        ((struct multi_sources_supp_info *) val)->fork_display = display_pipe_fork;
                        ((struct multi_sources_supp_info *) val)->state = state;
                        *len = sizeof(struct multi_sources_supp_info);
                        break;
                default:
                        return FALSE;
        }
        return TRUE;
}

int display_pipe_reconfigure(void *state, struct video_desc desc)
{
        struct state_pipe *s = (struct state_pipe *) state;

        s->desc = desc;

        return 1;
}

void display_pipe_put_audio_frame(void *state, struct audio_frame *frame)
{
        UNUSED(state);
        UNUSED(frame);
}

int display_pipe_reconfigure_audio(void *state, int quant_samples, int channels,
                int sample_rate)
{
        UNUSED(state);
        UNUSED(quant_samples);
        UNUSED(channels);
        UNUSED(sample_rate);

        return FALSE;
}

