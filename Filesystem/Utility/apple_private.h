//
//  apple_private.h
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

#ifndef lustre_apple_private_h
#define lustre_apple_private_h

#include <ipc/ipc_types.h>
#include <mach/mach_types.h>
#include <mach/error.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <kern/thread_call.h>
#include "apple_private_types.h"

#define LUSTRE_APPLE_PRIVATE(return, name, args) \
    typedef return  (*lustre_ ## name ## _def)args; \
    extern lustre_ ## name ## _def lustre_apple_private_ ## name;

#include "apple_private_functions.h"

kern_return_t lustre_apple_private_populate(void);

#endif /* lustre_apple_private_h */
