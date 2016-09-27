//
//  extensions.c
//  Filesystem
//
//  Lustre Filesystem For macOS
//  Copyright (C) 2016 Cider Apps, LLC.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <sys/errno.h>
#include "extensions.h"

errno_t lustre_extension_uio_move(void * addr, size_t size, uio_t uio)
{
    errno_t error;
    
    if (size > (size_t)uio_resid(uio)) {
        error = ENOBUFS;
    } else {
        error = uiomove(addr, (int)size, uio);
    }
    return error;
}
