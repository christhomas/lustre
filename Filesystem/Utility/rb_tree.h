//
//  rb_tree.h
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

#ifndef lustre_rb_tree_h
#define lustre_rb_tree_h

#include <mach/mach_types.h>
#include <stdint.h>
#include <sys/types.h>

static const uint16_t kLFSRbTreeHeightLimit     = 64;               // Tallest allowable tree

typedef int (* cmp_f) (const void * p1, const void * p2);

struct lustre_rb_tree_operations {
    void (* ref_count_inc)(void * data);
    void (* ref_count_dec)(void * data);
    int8_t (* comparator)(const void * data_a, const void * data_b);
    int8_t (* find_comparator)(const void * data, const void * key);
};

struct lustre_rb_tree_node {
    int                             red;                            // Color (1=red, 0=black)
    void *                          data;                           // Content
    struct lustre_rb_tree_node *    link[2];                        // Left (0) and right (1) links
};

struct lustre_rb_tree {
    struct lustre_rb_tree_node *        root;
    struct lustre_rb_tree_operations    operations;
    uint64_t                            size;
};

struct lustre_rb_tree_iterator {
    struct lustre_rb_tree *             tree;
    struct lustre_rb_tree_node *        current_node;
    struct lustre_rb_tree_node *        path[kLFSRbTreeHeightLimit];
    uint64_t                            top;                            // Top of stack
};

struct lustre_rb_tree *             lustre_rb_tree_alloc(struct lustre_rb_tree_operations operations);
void                                lustre_rb_tree_free(struct lustre_rb_tree * tree);
void *                              lustre_rb_tree_find(struct lustre_rb_tree * tree, void * data);
kern_return_t                       lustre_rb_tree_insert(struct lustre_rb_tree * tree, void * data);
kern_return_t                       lustre_rb_tree_remove(struct lustre_rb_tree * tree, void * data);
uint64_t                            lustre_rb_tree_count(struct lustre_rb_tree * tree);

struct lustre_rb_tree_iterator *    lustre_rb_tree_iterator_alloc(struct lustre_rb_tree * tree);
void                                lustre_rb_tree_iterator_free(struct lustre_rb_tree_iterator * iterator);
void *                              lustre_rb_tree_iterator_first(struct lustre_rb_tree_iterator * iterator);
void *                              lustre_rb_tree_iterator_last(struct lustre_rb_tree_iterator * iterator);
void *                              lustre_rb_tree_iterator_next(struct lustre_rb_tree_iterator * iterator);
void *                              lustre_rb_tree_iterator_prev(struct lustre_rb_tree_iterator * iterator);

#endif /* lustre_rb_tree_h */
