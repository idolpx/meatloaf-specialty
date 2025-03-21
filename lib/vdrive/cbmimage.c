/** \file   src/cbmimage.c
 * \brief   Generic image handling
 *
 * Right now only contains a single function to create either a disk or a tape
 * image file.
 */

/*
 * cbmimage.c - Generic image handling.
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

#include <stdio.h>

#include "diskimage.h"
#include "cbmimage.h"

/** \brief  Create a disk or tape image file
 *
 * \param[in]   name    name of/path to image
 * \param[in]   type    disk/tape image type enumerator
 *
 * \return  0 on success, < 0 on failure
 */
int cbmimage_create_image(const char *name, unsigned int type)
{
    return disk_image_fsimage_create(name, type);
}

/** \brief  Create a disk or tape image file
 *
 * \param[in]   name    name of/path to image
 * \param[in]   dname   disk name and id
 * \param[in]   type    disk/tape image type enumerator
 *
 * \return  0 on success, < 0 on failure
 */
int cbmimage_create_dxm_image(const char *name, const char *dname, unsigned int type)
{
    return disk_image_fsimage_create_dxm(name, dname, type);
}

/** \brief  Create a hard disk image file
 *
 * \param[in]   name    name of/path to image
 * \param[in]   dname   disk name and id
 * \param[in]   type    hard disk image type enumerator
 *
 * \return  0 on success, < 0 on failure
 */
int cbmimage_create_dhd_image(const char *name, const char *dname, unsigned int type)
{
    return disk_image_fsimage_create_dhd(name, dname, type);
}
