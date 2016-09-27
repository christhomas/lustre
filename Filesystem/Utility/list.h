//
//  list.h
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

#ifndef lustre_list_h
#define lustre_list_h

#include <mach/mach_types.h>
#include <stdint.h>
#include <sys/types.h>

struct lustre_list_operations {
    void (* ref_count_inc)(void * data);
    void (* ref_count_dec)(void * data);
};

struct lustre_list_entry {
    void *                      data;
    struct lustre_list_entry *  prev;
    struct lustre_list_entry *  next;
};

struct lustre_list {
    struct lustre_list_entry *      head;
    struct lustre_list_entry *      tail;
    struct lustre_list_operations   operations;
    uint64_t                        size;
    lck_mtx_t *                     mutex;
};

struct lustre_list *    lustre_list_alloc(struct lustre_list_operations operations);
void                    lustre_list_free(struct lustre_list * list);
kern_return_t           lustre_list_enqueue_head(struct lustre_list * list, void * data);
kern_return_t           lustre_list_enqueue_tail(struct lustre_list * list, void * data);
void *                  lustre_list_dequeue_head(struct lustre_list * list);
void *                  lustre_list_dequeue_tail(struct lustre_list * list);
void                    lustre_list_empty(struct lustre_list * list);
uint64_t                lustre_list_count(struct lustre_list * list);

#endif /* lustre_list_h */
