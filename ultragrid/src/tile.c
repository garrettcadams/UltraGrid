/*
 * FILE:    tile.c
 * AUTHORS: Martin Benes     <martinbenesh@gmail.com>
 *          Lukas Hejtmanek  <xhejtman@ics.muni.cz>
 *          Petr Holub       <hopet@ics.muni.cz>
 *          Milos Liska      <xliska@fi.muni.cz>
 *          Jiri Matela      <matela@ics.muni.cz>
 *          Dalibor Matura   <255899@mail.muni.cz>
 *          Ian Wesley-Smith <iwsmith@cct.lsu.edu>
 *
 * Copyright (c) 2005-2010 CESNET z.s.p.o.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 
 *      This product includes software developed by CESNET z.s.p.o.
 * 
 * 4. Neither the name of the CESNET nor the names of its contributors may be
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
 *
 */

#include "config.h"
#include "config_unix.h"
#include "config_win32.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "video_codec.h"
#include "tile.h"

#define MAGIC_H 0xFF
#define MAGIC_T 0xFE

void vf_split(struct video_frame *out, struct video_frame *src,
              unsigned int x_count, unsigned int y_count, int preallocate)
{
        unsigned int               tile_idx, line_idx;
        struct video_frame        *cur_tiles;
        unsigned int               tile_line;

        assert(src->width % x_count == 0u && src->height % y_count == 0u);

        for(tile_idx = 0u; tile_idx < x_count * y_count; ++tile_idx) {
                out[tile_idx].width = src->width / x_count;
                out[tile_idx].height = src->height / y_count;
                out[tile_idx].color_spec = src->color_spec;
                out[tile_idx].aux = src->aux | AUX_TILED;
                out[tile_idx].fps = src->fps;
                out[tile_idx].tile_info.x_count = x_count;
                out[tile_idx].tile_info.y_count = y_count;
                out[tile_idx].tile_info.pos_x = tile_idx % x_count;
                out[tile_idx].tile_info.pos_y = tile_idx / x_count;
                out[tile_idx].src_linesize = vc_getsrc_linesize(out[tile_idx].width,
                                src->color_spec);
                out[tile_idx].data_len = out[tile_idx].src_linesize * out[tile_idx].height;
        }

        cur_tiles = out;
        for(line_idx = 0u; line_idx < src->height; ++line_idx, ++tile_line) {
                unsigned int cur_tile_idx;
                unsigned int byte = 0u;

                if (line_idx % (src->height / y_count) == 0u) /* next tiles*/
                {
                        tile_line = 0u;
                        if (line_idx != 0u)
                                cur_tiles += x_count;
                        if (preallocate) {
                                for (cur_tile_idx = 0u; cur_tile_idx < x_count;
                                               ++cur_tile_idx) {
                                        cur_tiles[cur_tile_idx].data =
                                                malloc(cur_tiles[cur_tile_idx].
                                                                data_len);

                                }
                        }
                }

                for(cur_tile_idx = 0u; cur_tile_idx < x_count; ++cur_tile_idx) {
                        memcpy((void *) &cur_tiles[cur_tile_idx].data[
                                        tile_line *
                                        cur_tiles[cur_tile_idx].src_linesize],
                                        (void *) &src->data[line_idx *
                                        src->src_linesize + byte],
                                        cur_tiles[cur_tile_idx].width *
                                        get_bpp(src->color_spec));
                        byte += cur_tiles[cur_tile_idx].width * get_bpp(src->color_spec);
                }

        }
}

void vf_split_horizontal(struct video_frame *out, struct video_frame *src,
              unsigned int y_count)
{
        unsigned int i;

        for(i = 0u; i < y_count; ++i) {
                out[i].width = src->width;
                out[i].height = src->height / y_count;
                out[i].color_spec = src->color_spec;
                out[i].aux = src->aux | AUX_TILED;
                out[i].fps = src->fps;
                out[i].tile_info.x_count = 1u;
                out[i].tile_info.y_count = y_count;
                out[i].tile_info.pos_x = 0u;
                out[i].tile_info.pos_y = i;
                out[i].src_linesize = vc_getsrc_linesize(out[i].width,
                                src->color_spec);
                out[i].data_len = out[i].src_linesize * out[i].height;
                out[i].data = src->data + i * out[i].height * src->src_linesize;
        }
}


uint32_t hton_tileinfo2uint(struct tile_info tile_info)
{
        union {
                struct tile_info t_info;
                uint32_t res;
        } trans;
        trans.t_info = tile_info;
        trans.t_info.h_reserved = MAGIC_H;
        trans.t_info.t_reserved = MAGIC_T;
        return htonl(trans.res);
}

struct tile_info ntoh_uint2tileinfo(uint32_t packed)
{
        union {
                struct tile_info t_info;
                uint32_t src;
        } trans;
        trans.src = ntohl(packed);
        if(trans.t_info.h_reserved = MAGIC_H)
                return trans.t_info;
        else { /* == MAGIC_T */
                  int tmp;
                  tmp = trans.t_info.x_count << 4u & trans.t_info.y_count;
                  trans.t_info.x_count = trans.t_info.pos_x;
                  trans.t_info.y_count = trans.t_info.pos_y;
                  trans.t_info.pos_x = tmp >> 4u;
                  trans.t_info.pos_y = tmp & 0xF;
                  return trans.t_info;
        }
}

int tileinfo_eq_count(struct tile_info t1, struct tile_info t2)
{
        return t1.x_count == t2.x_count && t1.y_count == t2.y_count;
}
