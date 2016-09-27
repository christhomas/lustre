//
//  apple_private.c
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

#include <mach-o/loader.h>
#include <sys/systm.h>
#include "lustre.h"
#include "apple_private.h"
#include "assert.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define LUSTRE_APPLE_PRIVATE(return, name, args) \
    lustre_ ## name ## _def lustre_apple_private_ ## name;
#pragma clang diagnostic pop

#include "apple_private_functions.h"

extern void vm_kernel_unslide_or_perm_external(vm_offset_t addr, vm_offset_t *up_addr);

struct nlist_64 {
    union {
        uint32_t  n_strx; /* index into the string table */
    } n_un;
    uint8_t n_type;        /* type flag, see below */
    uint8_t n_sect;        /* section number or NO_SECT */
    uint16_t n_desc;       /* see <mach-o/stab.h> */
    uint64_t n_value;      /* value of this symbol (or stab offset) */
};

struct segment_command_64 * find_segment_64(struct mach_header_64 * mh, const char * segname)
{
    struct load_command *lc;
    struct segment_command_64 *seg, *foundseg = NULL;

    /* First LC begins straight after the mach header */
    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));
    while ((uint64_t)lc < (uint64_t)mh + (uint64_t)mh->sizeofcmds) {
        if (lc->cmd == LC_SEGMENT_64) {
            /* Check load command's segment name */
            seg = (struct segment_command_64 *)lc;
            if (strcmp(seg->segname, segname) == 0) {
                foundseg = seg;
                break;
            }
        }
        
        /* Next LC */
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }

    /* Return the segment (NULL if we didn't find it) */
    return foundseg;
}

struct load_command * find_load_command(struct mach_header_64 * mh, uint32_t cmd)
{
    struct load_command *lc, *foundlc;
    
    /* First LC begins straight after the mach header */
    lc = (struct load_command *)((uint64_t)mh + sizeof(struct mach_header_64));
    while ((uint64_t)lc < (uint64_t)mh + (uint64_t)mh->sizeofcmds) {
        if (lc->cmd == cmd) {
            foundlc = (struct load_command *)lc;
            break;
        }
        
        /* Next LC */
        lc = (struct load_command *)((uint64_t)lc + (uint64_t)lc->cmdsize);
    }
    
    /* Return the load command (NULL if we didn't find it) */
    return foundlc;
}

void * lustre_apple_private_find_symbol(struct mach_header_64 * mh, const char * name)
{
    struct symtab_command *symtab = NULL;
    struct segment_command_64 *linkedit = NULL;
    void *strtab = NULL;
    struct nlist_64 *nl = NULL;
    char *str;
    uint64_t i;
    void *addr = NULL;
    
    /*
     * Check header
     */
    if (mh->magic != MH_MAGIC_64) {
        printf("FAIL: magic number doesn't match - 0x%x\n", mh->magic);
        return NULL;
    }
    
    /*
     * Find the LINKEDIT and SYMTAB sections
     */
    linkedit = find_segment_64(mh, SEG_LINKEDIT);
    if (!linkedit) {
        printf("FAIL: couldn't find __LINKEDIT\n");
        return NULL;
    }
    
    symtab = (struct symtab_command *)find_load_command(mh, LC_SYMTAB);
    if (!symtab) {
        printf("FAIL: couldn't find SYMTAB\n");
        return NULL;
    }
    
    /*
     * Enumerate symbols until we find the one we're after
     */
    strtab = (void *)((int64_t)linkedit->vmaddr + (int64_t)linkedit->vmsize - symtab->strsize);
    for (i = 0, nl = (struct nlist_64 *)((int64_t)linkedit->vmaddr + (int64_t)linkedit->vmsize - symtab->strsize - symtab->nsyms*sizeof(struct nlist_64));
         i < symtab->nsyms;
         i++, nl = (struct nlist_64 *)((uint64_t)nl + sizeof(struct nlist_64)))
    {
        str = (char *)(strtab + nl->n_un.n_strx);
        if (strcmp(str, name) == 0) {
            addr = (void *)nl->n_value;
            break;
        }
    }
    
    /* Return the address (NULL if we didn't find it) */
    return addr;
}

kern_return_t lustre_apple_private_populate(void)
{
    uintptr_t               printf_address;
    uintptr_t               printf_address_slide;
    uintptr_t               slide;
    struct mach_header_64 * mach_header;
    
    printf_address          = (uintptr_t)(void *)printf;
    printf_address_slide    = 0;

    vm_kernel_unslide_or_perm_external(printf_address, &printf_address_slide);

    slide       = printf_address - printf_address_slide;
    mach_header = (struct mach_header_64 *)(0xffffff8000200000 + slide);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define LUSTRE_APPLE_PRIVATE_SYMBOL(name) _ ## name
#define LUSTRE_APPLE_PRIVATE_XSTR(s) LUSTRE_APPLE_PRIVATE_STR(s)
#define LUSTRE_APPLE_PRIVATE_STR(s) #s
#define LUSTRE_APPLE_PRIVATE(_return, name, args) \
    lustre_apple_private_ ## name = lustre_apple_private_find_symbol(mach_header, LUSTRE_APPLE_PRIVATE_XSTR(LUSTRE_APPLE_PRIVATE_SYMBOL(name))); \
    if (!lustre_apple_private_ ## name) { \
        printf("Can't find needed function: %s\n", LUSTRE_APPLE_PRIVATE_XSTR(LUSTRE_APPLE_PRIVATE_SYMBOL(name))); \
        return KERN_FAILURE; \
    }
#pragma clang diagnostic pop

#include "apple_private_functions.h"
    
    return KERN_SUCCESS;
}
