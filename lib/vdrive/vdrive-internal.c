/*
 * vdrive-internal.c - Virtual disk-drive implementation.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *
 * Multi-drive and DHD enhancements by
 *  Roberto Muscedere <rmusced@uwindsor.ca>
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

#include <stdio.h>
#include <stdlib.h>

#include "cbmdos.h"
#include "diskimage.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "vdrive-command.h"
#include "vdrive-internal.h"
#include "vdrive.h"
#include "p64.h"


static log_t vdrive_internal_log = LOG_DEFAULT;


/*
Called by 2 things:
disk preview tool - read_only mode is 1 (imagecontents/diskcontents.c)
disk create tool - read_only is 0 (in this file)
when read_only is 0, the image is expected to be created regardless of
errors.
*/
vdrive_t *vdrive_internal_open_fsimage(const char *name, unsigned int read_only)
{
    vdrive_t *vdrive;
    disk_image_t *image;
    int ret;

    image = lib_malloc(sizeof(disk_image_t));

    image->gcr = NULL;
    image->p64 = lib_calloc(1, sizeof(TP64Image));
    P64ImageCreate((void*)image->p64);
    image->read_only = read_only;

    image->device = DISK_IMAGE_DEVICE_FS;

    disk_image_media_create(image);

    disk_image_name_set(image, name);

    if (disk_image_open(image) < 0) {
        disk_image_media_destroy(image);
        P64ImageDestroy((void*)image->p64);
        lib_free(image->p64);
        lib_free(image);
        log_error(vdrive_internal_log, "Cannot open file `%s'", name);
        return NULL;
    }

    vdrive = lib_calloc(1, sizeof(vdrive_t));

    vdrive_device_setup(vdrive, 100);
    vdrive->image = image;
    ret = vdrive_attach_image(image, 100, 0, vdrive);

    /* if we can't attached to it IN READ MODE, we should return NULL */
    if (ret && read_only) {
        vdrive_device_shutdown(vdrive);
        lib_free(vdrive);
        disk_image_media_destroy(image);
        P64ImageDestroy((void*)image->p64);
        lib_free(image->p64);
        lib_free(image);
        return NULL;
    }

    /* otherwise return the vdrive context, hoping everything will work out */
    return vdrive;
}

int vdrive_internal_close_disk_image(vdrive_t *vdrive)
{
    disk_image_t *image = vdrive->image;

    if (vdrive->unit != 8 && vdrive->unit != 9 && vdrive->unit != 10 && vdrive->unit != 11) {
        vdrive_detach_image(image, 100, 0, vdrive);

        if (disk_image_close(image) < 0) {
            return -1;
        }

        P64ImageDestroy((void*)image->p64);

        disk_image_media_destroy(image);
        vdrive_device_shutdown(vdrive);
        lib_free(image->p64);
        lib_free(image);
        lib_free(vdrive);
    }

    return 0;
}

static int vdrive_internal_format_disk_image(const char *filename,
                                             const char *disk_name)
{
    vdrive_t *vdrive;
    const char *format_name;
    int status = 0;

    format_name = (disk_name == NULL) ? " " : disk_name;

    /* FIXME: Pass unit here.  */
    //machine_drive_flush();
    vdrive = vdrive_internal_open_fsimage(filename, 0);

    if (vdrive == NULL) {
        return -1;
    }

    if (vdrive_command_format(vdrive, format_name) != CBMDOS_IPE_OK) {
        status = -1;
    }

    if (vdrive_internal_close_disk_image(vdrive) < 0) {
        return -1;
    }

    return status;
}

int vdrive_internal_create_format_disk_image(const char *filename,
                                             const char *diskname,
                                             unsigned int type)
{
    switch (type) {
        case DISK_IMAGE_TYPE_D1M:
        case DISK_IMAGE_TYPE_D2M:
        case DISK_IMAGE_TYPE_D4M:
            return disk_image_fsimage_create_dxm(filename, diskname, type);
            break;
        case DISK_IMAGE_TYPE_DHD:
            return disk_image_fsimage_create_dhd(filename, diskname, type);
        default:
            if (disk_image_fsimage_create(filename, type) < 0) {
                return -1;
            }
            if (vdrive_internal_format_disk_image(filename, diskname) < 0) {
                return -1;
            }
    }

    return 0;
}

void vdrive_internal_init(void)
{
    vdrive_internal_log = log_open("VDrive Internal");
}
