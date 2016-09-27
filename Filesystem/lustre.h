//
//  lustre.h
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

#ifndef lustre_lustre_h
#define lustre_lustre_h

#include <libkern/OSMalloc.h>
#include <libkern/locks.h>
#include <pexpert/pexpert.h>

// used for all of our allocations.
extern OSMallocTag  lustre_os_malloc_tag;

// used for all of our locks.
extern lck_grp_t *  lustre_lock_group;

#endif /* lustre_lustre_h */
