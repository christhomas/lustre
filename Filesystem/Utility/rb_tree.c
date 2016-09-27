//
//  rb_tree.c
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
#include <string.h>
#include "lustre.h"
#include "rb_tree.h"
#include "assert.h"
#include "logging.h"

#pragma mark - Internal

struct lustre_rb_tree_node * lustre_rb_tree_new_node(struct lustre_rb_tree * tree, void * data)
{
    struct lustre_rb_tree_node * node;
    
    LUSTRE_BUG_ON(!tree);
    LUSTRE_BUG_ON(!data);
    
    node = (struct lustre_rb_tree_node *)OSMalloc(sizeof(struct lustre_rb_tree_node), lustre_os_malloc_tag);
    
    if (node) {
        node->red       = 1;
        node->data      = data;
        node->link[0]   = NULL;
        node->link[1]   = NULL;
        
        tree->operations.ref_count_inc(data);
    } else {
        os_log_error(lustre_logger_utility, "Failed to allocate node");
    }
    
    return node;
}

uint8_t lustre_rb_tree_is_red(struct lustre_rb_tree_node * node)
{
    return (node && (node->red == 1));
}

struct lustre_rb_tree_node * lustre_rb_tree_single(struct lustre_rb_tree_node * node, uint8_t dir)
{
    struct lustre_rb_tree_node * save;
    
    LUSTRE_BUG_ON(!node);
    
    save                = node->link[!dir];
    node->link[!dir]    = save->link[dir];
    save->link[dir]     = node;
    node->red           = 1;
    save->red           = 0;
    
    return save;
}

struct lustre_rb_tree_node * lustre_rb_tree_double(struct lustre_rb_tree_node * node, uint8_t dir)
{
    LUSTRE_BUG_ON(!node);
    
    node->link[!dir] = lustre_rb_tree_single(node->link[!dir], !dir);
    
    return lustre_rb_tree_single(node, dir);
}

struct lustre_rb_tree_node * lustre_rb_tree_iterator_start(struct lustre_rb_tree_iterator * iterator, uint8_t dir)
{
    LUSTRE_BUG_ON(!iterator);
    
    iterator->current_node  = iterator->tree->root;
    iterator->top           = 0;
    
    // Save the path for later traversal
    if (iterator->current_node) {
        while (iterator->current_node->link[dir]) {
            iterator->path[iterator->top++] = iterator->current_node;
            iterator->current_node          = iterator->current_node->link[dir];
        }
    }
    
    return iterator->current_node;
}

struct lustre_rb_tree_node * lustre_rb_tree_iterator_move(struct lustre_rb_tree_iterator * iterator, uint8_t dir)
{
    struct lustre_rb_tree_node * last;
    
    LUSTRE_BUG_ON(!iterator);
    
    if (iterator->current_node->link[dir]) {
        // Continue down this branch
        iterator->path[iterator->top++] = iterator->current_node;
        iterator->current_node          = iterator->current_node->link[dir];
        
        while (iterator->current_node->link[!dir]) {
            iterator->path[iterator->top++] = iterator->current_node;
            iterator->current_node          = iterator->current_node->link[!dir];
        }
    } else {
        // Move to the next branch
        do {
            if (iterator->top == 0) {
                iterator->current_node = NULL;
                break;
            }
            
            last                    = iterator->current_node;
            iterator->top           -= 1;
            iterator->current_node  = iterator->path[iterator->top];
        } while (last == iterator->current_node->link[dir]);
    }
    
    return iterator->current_node;
}

#pragma mark - External

struct lustre_rb_tree * lustre_rb_tree_alloc(struct lustre_rb_tree_operations operations)
{
    struct lustre_rb_tree * tree;
    
    tree = (struct lustre_rb_tree *)OSMalloc(sizeof(struct lustre_rb_tree), lustre_os_malloc_tag);
    if (tree) {
        tree->operations    = operations;
        tree->root          = NULL;
        tree->size          = 0;
    } else {
        os_log_error(lustre_logger_utility, "Failed to allocate tree");
    }
    
    return tree;
}

void lustre_rb_tree_free(struct lustre_rb_tree * tree)
{
    struct lustre_rb_tree_iterator *    iterator;
    struct lustre_rb_tree_node *        node;
    
    LUSTRE_BUG_ON(!tree);
    
    iterator = lustre_rb_tree_iterator_alloc(tree);
    if (!iterator) {
        os_log_error(lustre_logger_utility, "Failed to allocate iterator - not freeing nodes");
        return;
    }
    
    node = lustre_rb_tree_iterator_start(iterator, 1);
    while (node) {
        tree->operations.ref_count_dec(node->data);
        OSFree(node, sizeof(struct lustre_rb_tree_node), lustre_os_malloc_tag);
        node = lustre_rb_tree_iterator_move(iterator, 0);
    }
    
    OSFree(tree, sizeof(struct lustre_rb_tree), lustre_os_malloc_tag);
}

void * lustre_rb_tree_find(struct lustre_rb_tree * tree, void * data)
{
    struct lustre_rb_tree_node *    node;
    uint8_t                         comparison_result;
    void *                          result;
    
    LUSTRE_BUG_ON(!tree);
    
    node    = tree->root;
    result  = NULL;
    
    while (node) {
        comparison_result = tree->operations.find_comparator(node->data, data);
        
        if (comparison_result == 0) {
            break;
        }
        
        node = node->link[comparison_result < 0];
    }
    
    if (node) {
        result = node->data;
    }
    
    return result;
}

kern_return_t lustre_rb_tree_insert(struct lustre_rb_tree * tree, void * data)
{
    kern_return_t                   result;
    struct lustre_rb_tree_node      head;  // false tree root
    struct lustre_rb_tree_node *    g;     // grandparent
    struct lustre_rb_tree_node *    t;     // parent
    struct lustre_rb_tree_node *    p;     // iterator
    struct lustre_rb_tree_node *    q;     // parent
    uint8_t                         dir;
    uint8_t                         dir2;
    uint8_t                         last;
    
    LUSTRE_BUG_ON(!tree);
    LUSTRE_BUG_ON(!data);
    
    result = KERN_SUCCESS;
    
    if (!tree->root) {
        tree->root = lustre_rb_tree_new_node(tree, data);
        
        if (!tree->root) {
            result = KERN_NO_SPACE;
        }
    } else {
        bzero(&head, sizeof(struct lustre_rb_tree_node));
        
        dir         = 0;
        last        = 0;
        t           = &head;
        g           = NULL;
        p           = NULL;
        q           = tree->root;
        t->link[1]  = tree->root;
        
        // Search down the tree for a place to insert
        while (1) {
            if (!q) {
                // Insert a new node at the first null link
                q = lustre_rb_tree_new_node(tree, data);
                if (!q) {
                    result = KERN_NO_SPACE;
                    break;
                }
                p->link[dir] = q;
            } else if (lustre_rb_tree_is_red(q->link[0]) && lustre_rb_tree_is_red(q->link[1])) {
                // Simple red violation: color flip
                q->red          = 1;
                q->link[0]->red = 0;
                q->link[1]->red = 0;
            }
            
            if (lustre_rb_tree_is_red(q) && lustre_rb_tree_is_red(p)) {
                // Hard red violation: rotations necessary
                dir2 = (t->link[1] == g);
                
                if (q == p->link[last]) {
                    t->link[dir2] = lustre_rb_tree_single(g, !last);
                } else {
                    t->link[dir2] = lustre_rb_tree_single(g, !last);
                }
            }
            
            // Stop working if we inserted a node (also disallows duplicates in the tree)
            if (tree->operations.comparator(q->data, data) == 0) {
                break;
            }
            
            last    = dir;
            dir     = (tree->operations.comparator(q->data, data) < 0);
            
            // Move the helpers down
            if (g) {
                t = g;
            }
            
            g = p;
            p = q;
            q = q->link[dir];
        }
        
        // Update the root (it may be different)
        tree->root = head.link[1];
    }
    
    if (result != KERN_SUCCESS) {
        tree->root->red = 0;
        tree->size      += 1;
    }
    
    return result;
}

kern_return_t lustre_rb_tree_remove(struct lustre_rb_tree * tree, void * data)
{
    kern_return_t                   result;
    struct lustre_rb_tree_node      head;
    struct lustre_rb_tree_node *    q;
    struct lustre_rb_tree_node *    p;
    struct lustre_rb_tree_node *    g;
    struct lustre_rb_tree_node *    f;
    struct lustre_rb_tree_node *    s;
    uint8_t                         dir;
    uint8_t                         dir2;
    uint8_t                         last;
    
    LUSTRE_BUG_ON(!tree);
    
    bzero(&head, sizeof(struct lustre_rb_tree_node));
    
    result      = KERN_SUCCESS;
    f           = NULL;
    dir         = 1;
    q           = &head;
    g           = NULL;
    p           = NULL;
    q->link[1]  = tree->root;
    
    // Search and push a red node down to fix red violations as we go
    while (q->link[dir]) {
        last = dir;
        
        // Move the helpers down
        g   = p;
        p   = q;
        q   = q->link[dir];
        dir = (tree->operations.comparator(q->data, data) < 0);
        
        // Save the node with matching data and keep going; we'll do removal tasks at the end
        if (tree->operations.comparator(q->data, data) == 0) {
            f = q;
        }
        
        // Push the red node down with rotations and color flips
        if (!lustre_rb_tree_is_red(q) && !lustre_rb_tree_is_red(q->link[dir])) {
            if (lustre_rb_tree_is_red(q->link[!dir])) {
                p->link[last]   = lustre_rb_tree_single(q, dir);
                p               = p->link[last];
            } else if (!lustre_rb_tree_is_red(q->link[!dir])) {
                s = p->link[!last];
                
                if (s) {
                    if (!lustre_rb_tree_is_red(s->link[!last]) && !lustre_rb_tree_is_red(s->link[last])) {
                        // Color flip
                        p->red = 0;
                        s->red = 1;
                        q->red = 1;
                    } else {
                        dir2 = (g->link[1] == p);
                        
                        if (lustre_rb_tree_is_red(s->link[last])) {
                            g->link[dir2] = lustre_rb_tree_double(p, last);
                        } else if (lustre_rb_tree_is_red(s->link[!last])) {
                            g->link[dir2] = lustre_rb_tree_single(p, last);
                        }
                        // Ensure correct coloring
                        q->red                      = 1;
                        g->link[dir2]->red          = 1;
                        g->link[dir2]->link[0]->red = 0;
                        g->link[dir2]->link[1]->red = 0;
                    }
                }
            }
        }
    }
    
    // Replace and remove the saved node
    if (f) {
        tree->operations.ref_count_dec(f->data);
        f->data                     = q->data;
        p->link[p->link[1] == q]    = (q->link[q->link[0] == NULL]);
        OSFree(q, sizeof(struct lustre_rb_tree_node), lustre_os_malloc_tag);
    } else {
        result = KERN_INVALID_ARGUMENT;
    }
    
    // Update the root (it may be different)
    tree->root = head.link[1];
    
    // Make the root black for simplified logic
    if (tree->root) {
        tree->root->red = 0;
    }
    
    tree->size -= 1;
    
    return result;
}

uint64_t lustre_rb_tree_count(struct lustre_rb_tree * tree)
{
    LUSTRE_BUG_ON(!tree);
    
    return tree->size;
}

struct lustre_rb_tree_iterator * lustre_rb_tree_iterator_alloc(struct lustre_rb_tree * tree)
{
    struct lustre_rb_tree_iterator * iterator;
    
    LUSTRE_BUG_ON(!tree);
    
    iterator = (struct lustre_rb_tree_iterator *)OSMalloc(sizeof(struct lustre_rb_tree_iterator), lustre_os_malloc_tag);
    if (iterator) {
        iterator->tree          = tree;
        iterator->current_node  = NULL;
        iterator->path[0]       = NULL;
        iterator->top           = 0;
    } else {
        os_log_error(lustre_logger_utility, "Failed to allocate iterator");
    }
    
    return iterator;
}

void lustre_rb_tree_iterator_free(struct lustre_rb_tree_iterator * iterator)
{
    LUSTRE_BUG_ON(!iterator);
    
    OSFree(iterator, sizeof(struct lustre_rb_tree_iterator), lustre_os_malloc_tag);
}

void * lustre_rb_tree_iterator_first(struct lustre_rb_tree_iterator * iterator)
{
    struct lustre_rb_tree_node * node;
    
    LUSTRE_BUG_ON(!iterator);
    
    node = lustre_rb_tree_iterator_start(iterator, 0);
    
    if (node) {
        return node->data;
    } else {
        return NULL;
    }
}

void * lustre_rb_tree_iterator_last(struct lustre_rb_tree_iterator * iterator)
{
    struct lustre_rb_tree_node * node;
    
    LUSTRE_BUG_ON(!iterator);
    
    node = lustre_rb_tree_iterator_start(iterator, 1);
    
    if (node) {
        return node->data;
    } else {
        return NULL;
    }
}

void * lustre_rb_tree_iterator_next(struct lustre_rb_tree_iterator * iterator)
{
    struct lustre_rb_tree_node * node;
    
    LUSTRE_BUG_ON(!iterator);
    
    node = lustre_rb_tree_iterator_move(iterator, 1);
    
    if (node) {
        return node->data;
    } else {
        return NULL;
    }
}

void * lustre_rb_tree_iterator_prev(struct lustre_rb_tree_iterator * iterator)
{
    struct lustre_rb_tree_node * node;
    
    LUSTRE_BUG_ON(!iterator);
    
    node = lustre_rb_tree_iterator_move(iterator, 0);
    
    if (node) {
        return node->data;
    } else {
        return NULL;
    }
}
