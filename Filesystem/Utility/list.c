//
//  list.c
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

#include <pexpert/pexpert.h>
#include <string.h>
#include "lustre.h"
#include "list.h"
#include "assert.h"
#include "logging.h"

#pragma mark - Private

struct lustre_list_entry * lustre_list_entry_alloc(struct lustre_list * list, void * data)
{
    struct lustre_list_entry * entry;
    
    LUSTRE_BUG_ON(!list);
    LUSTRE_BUG_ON(!data);
    
    entry = (struct lustre_list_entry *)OSMalloc(sizeof(struct lustre_list_entry), lustre_os_malloc_tag);
    if (entry) {
        list->operations.ref_count_inc(data);
        entry->data = data;
        entry->next = NULL;
        entry->prev = NULL;
    } else {
        os_log_error(lustre_logger_utility, "Failed to allocate list entry");
    }
    
    return entry;
}

void lustre_list_entry_free(struct lustre_list * list, struct lustre_list_entry * entry)
{
    LUSTRE_BUG_ON(!list);
    LUSTRE_BUG_ON(!entry);
    
    OSFree(entry, sizeof(struct lustre_list_entry), lustre_os_malloc_tag);
}

#pragma mark - Public

struct lustre_list * lustre_list_alloc(struct lustre_list_operations operations)
{
    struct lustre_list * list;
    
    list = (struct lustre_list *)OSMalloc(sizeof(struct lustre_list), lustre_os_malloc_tag);
    if (list) {
        list->mutex = lck_mtx_alloc_init(lustre_lock_group, LCK_ATTR_NULL);
        if (!list->mutex) {
            os_log_error(lustre_logger_utility, "Failed to allocate list mutex");
            OSFree(list, sizeof(struct lustre_list), lustre_os_malloc_tag);
            list = NULL;
        } else {
            list->operations    = operations;
            list->head          = NULL;
            list->tail          = NULL;
            list->size          = 0;
        }
    } else {
        os_log_error(lustre_logger_utility, "Failed to allocate list");
    }
    
    return list;
}

void lustre_list_free(struct lustre_list * list)
{
    struct lustre_list_entry * entry;
    
    LUSTRE_BUG_ON(!list);
    LUSTRE_BUG_ON(!list->mutex);
    
    while ((entry = lustre_list_dequeue_head(list)));
    lck_mtx_free(list->mutex, lustre_lock_group);
    OSFree(list, sizeof(struct lustre_list), lustre_os_malloc_tag);
}

kern_return_t lustre_list_enqueue_head(struct lustre_list * list, void * data)
{
    struct lustre_list_entry *  entry;
    kern_return_t               result;
    
    LUSTRE_BUG_ON(!list);
    LUSTRE_BUG_ON(!list->mutex);
    LUSTRE_BUG_ON(!data);
    
    result = KERN_SUCCESS;
    
    entry = lustre_list_entry_alloc(list, data);
    if (!entry) {
        result = KERN_FAILURE;
        goto end;
    }

    lck_mtx_lock(list->mutex);
    if (list->head) {
        entry->next = list->head;
    }
    list->head = entry;
    if (!list->tail) {
        list->tail = list->head;
    }
    list->size += 1;
    lck_mtx_unlock(list->mutex);

end:
    return result;
}

kern_return_t lustre_list_enqueue_tail(struct lustre_list * list, void * data)
{
    struct lustre_list_entry *  entry;
    kern_return_t               result;
    
    LUSTRE_BUG_ON(!list);
    LUSTRE_BUG_ON(!list->mutex);
    LUSTRE_BUG_ON(!data);
    
    result = KERN_SUCCESS;
    
    entry = lustre_list_entry_alloc(list, data);
    if (!entry) {
        result = KERN_FAILURE;
        goto end;
    }
    
    lck_mtx_lock(list->mutex);
    if (list->tail) {
        list->tail->next = entry;
    }
    list->tail = entry;
    if (!list->head) {
        list->head = list->tail;
    }
    list->size += 1;
    lck_mtx_unlock(list->mutex);
    
end:
    return result;
}

void * lustre_list_dequeue_head(struct lustre_list * list)
{
    struct lustre_list_entry *  entry;;
    void *                      data;
    
    LUSTRE_BUG_ON(!list);
    
    data = NULL;
    
    lck_mtx_lock(list->mutex);
    if (list->head) {
        entry = list->head;
        list->head = entry->next;
        if ((list->head) && (list->head->next)) {
            list->head->next->prev = list->head;
        }
        
        if (list->tail == entry) {
            list->tail = list->head;
        }
        
        data = entry->data;
        lustre_list_entry_free(list, entry);
        list->size -= 1;
    }
    lck_mtx_unlock(list->mutex);
    
    return data;
}

void * lustre_list_dequeue_tail(struct lustre_list * list)
{
    struct lustre_list_entry *  entry;
    void *                      data;
    
    LUSTRE_BUG_ON(!list);
    
    data = NULL;
    
    lck_mtx_lock(list->mutex);
    if (list->tail) {
        entry = list->tail;
        list->tail = entry->prev;
        if ((list->tail) && (list->tail->prev)) {
            list->tail->prev->next = list->tail;
        }
        
        if (list->head == entry) {
            list->head = list->tail;
        }
        
        data = entry->data;
        lustre_list_entry_free(list, entry);
        list->size -= 1;
    }
    lck_mtx_unlock(list->mutex);
    
    return data;
}

void lustre_list_empty(struct lustre_list * list)
{
    struct lustre_list_entry * entry;
    
    LUSTRE_BUG_ON(!list);
    
    lck_mtx_lock(list->mutex);
    entry = list->head;
    
    while (entry) {
        lustre_list_entry_free(list, entry);
        list->operations.ref_count_dec(entry->data);
        entry = entry->next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    lck_mtx_unlock(list->mutex);
}

uint64_t lustre_list_count(struct lustre_list * list)
{
    uint64_t count;
    
    LUSTRE_BUG_ON(!list);
    LUSTRE_BUG_ON(!list->mutex);
    
    lck_mtx_lock(list->mutex);
    count = list->size;
    lck_mtx_unlock(list->mutex);

    return count;
}
