/*
 * fsimage.h
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef VICE_FSIMAGE_H
#define VICE_FSIMAGE_H

#include <stdio.h>

#include "types.h"
#include "archdep.h"

struct disk_image_s;
struct disk_addr_s;

typedef struct fsimage_s {
    ADFILE *fd;
    char *name;
    struct {
        uint8_t *map;
        int dirty;
        int len;
    } error_info;
} fsimage_t;


void fsimage_init(void);

void fsimage_name_set(struct disk_image_s *image, const char *name);
const char *fsimage_name_get(const struct disk_image_s *image);
ADFILE *fsimage_fd_get(const disk_image_t *image);
void fsimage_media_create(struct disk_image_s *image);
void fsimage_media_destroy(struct disk_image_s *image);

int fsimage_open(struct disk_image_s *image);
int fsimage_close(struct disk_image_s *image);
int fsimage_read_sector(const struct disk_image_s *image, uint8_t *buf,
                        const struct disk_addr_s *dadr);
int fsimage_write_sector(struct disk_image_s *image, const uint8_t *buf,
                         const struct disk_addr_s *dadr);
off_t fsimage_size(const disk_image_t *image);

#endif
