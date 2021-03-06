/*
 * Copyright (C) 2000-2002 Serge Vakulenko <vak@cronyx.ru>
 * Originally based on sources of Liquorice operating system
 * by David J. Hudson <dave@humbug.demon.co.uk>, heavily rewritten.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * Lesser General Public License (LGPL) as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your discretion) any
 * later version.  See the accompanying file "copying-lgpl.txt" for more
 * details.
 */
/** \namespace mem \brief \~english
 * Memory allocation routines.
 *
 * The strategy used within this memory allocator is to try and cause as
 * little waste as possible.  This can make things a little slower than
 * would be ideal, but does give the best chance of things keeping running.
 */

/** \namespace mem \brief \~russian
 * Процедуры выделения памяти.
 *
 * Свободная память хранится в виде списка, упорядоченного по возрастанию адресов.
 * Выделяется первый сегмент, подходящий по размеру.
 */
#include <uos-conf.h>
#include <uos-conf-cpu.h>
#include <runtime/lib.h>
#include <mem/mem.h>

/*
 * Debug configuration.
 */

#ifdef NDEBUG			/* Disable memory debugging on NDEBUG */
#undef MEM_DEBUG
#endif

#ifndef MEM_DEBUG		/* By default memory debugging is disabled */
#define MEM_DEBUG		0
#endif

/*
 * Memory alignment.
 * Align data on pointer-sized boundaries.
 */
#define SIZEOF_POINTER		sizeof(void*)
#define SIZEOF_DOUBLE       sizeof(double)

#if defined(MIPS32) && defined(ARCH_HAVE_FPU)
//!!! mips platform requires that all doubles be aligned on 8byte
//  , so allocate always on this aligns
#define SIZEOF_ALIGN    SIZEOF_DOUBLE
#else
#define SIZEOF_ALIGN    SIZEOF_POINTER
#endif

#define MASK_ALIGN      (SIZEOF_ALIGN-1)
#define MEM_ALIGN(x)		((((size_t)x) + MASK_ALIGN) & ~MASK_ALIGN)

/*
 * Every memory block has a header.
 */
typedef struct {
	size_t size;			/* Block size including the header */
	mem_pool_t *pool;		/* Memory pool pointer */
#if MEM_DEBUG
	unsigned short magic;		/* For data curruption test */
#define MEMORY_HOLE_MAGIC	0x4d48	/* Free memory block (hole) */
#define MEMORY_BLOCK_MAGIC	0x4d42	/* Memory block in use */
#endif
	//mheader_t* next ;     //use macro NEXT instead!!! used by holes only
} mheader_t;

/*
 * In memory holes (free blocks), the space just after the header
 * is used as a pointer to the next hole (linked free list).
 */
/*
static inline
mheader_t& NEXT(mheader_t* h){
    return (*(mheader_t**) ((h) + 1));
}
*/
#define NEXT(h)			(*(mheader_t**) ((h) + 1))

static inline
mheader_t* SUCC(mheader_t* h){
    return ( (mheader_t*)((size_t)h + h->size) );
}
//#define SUCC(h)         ( (mheader_t*)((size_t)h + h->size) )

#define MEM_HSIZE       MEM_ALIGN( sizeof(mheader_t) )
static inline
void* BLOCK_OF(mheader_t* h){
    return ((void*)((size_t)h + MEM_HSIZE));
}

static inline
mheader_t* H_OF(void* block){
    return ((mheader_t*)((size_t)block - MEM_HSIZE));
}
//#define H_OF(block) ((mheader_t*)((size_t)block - MEM_HSIZE))

#if MEM_DEBUG > 0
void mem_validate_hole(mem_pool_t *m, mheader_t *h);
#else
#define mem_validate_hole(...)
#endif

/**
 * Allocate a block of memory.
 * Fill it with zeroes.
 */
void *mem_alloc (mem_pool_t *m, size_t required)
{
	void *p;

	p = mem_alloc_dirty (m, required);
	if (p && required > 0)
		memsetw (p, 0, required);
	return p;
}

/**
 * Allocate a block of memory.
 * The memory may contain garbage.
 */
void *mem_alloc_dirty (mem_pool_t *m, size_t required)
{
	mheader_t *h, **hprev, *newh;

        /* All allocations need to be several bytes larger than the
         * amount requested by our caller.  They also need to be large enough
         * that they can contain a "mheader_t" and any magic values used in
         * debugging (for when the block gets freed and becomes an isolated
         * hole). */
	if (required < SIZEOF_POINTER)
		required = SIZEOF_POINTER;
	required = MEM_ALIGN (required + MEM_HSIZE);

	mutex_lock (&m->lock);

	/* Scan the list of all available memory holes and find the first
	 * one that meets our requirement. */
	h = (mheader_t*) m->free_list;
	hprev = (mheader_t**) (void*) &m->free_list;
	while (h) {
	    mem_validate_hole(m, h);
        	if (h->size >= required)
        		break;

		hprev = &NEXT(h);
		h = NEXT(h);
	}

        /* Did we find any space available? */
        if (! h) {
		mutex_unlock (&m->lock);
		/*debug_printf ("mem_alloc failed, size=%d bytes\n", required);*/
		return 0;
	}

        /* Remove a chunk of space and, if we can, release any of what's left
	 * as a new hole.  If we can't release any then allocate more than was
	 * requested and remove this hole from the hole list. */
	if (h->size >= required + MEM_HSIZE + 2*SIZEOF_POINTER) {
		newh = (mheader_t*) ((size_t)h + required);
		newh->pool = h->pool;
		newh->size = h->size - required;
#if MEM_DEBUG
        newh->magic = MEMORY_HOLE_MAGIC;
#endif
        NEXT(newh) = NEXT(h);
		h->size = required;
	} else {
	    newh = NEXT(h);
	}
    *hprev = newh;
#if MEM_DEBUG
	h->magic = MEMORY_BLOCK_MAGIC;
#endif
	m->free_size -= h->size;
	mutex_unlock (&m->lock);
	/*debug_printf ("mem %d bytes returned 0x%x\n", h->size, h+1);*/
	return BLOCK_OF(h);
}

/*
 * Add new hole to the free list.
 * Must be called with poll locked.
 */
static void make_hole_locked (mheader_t *newh)
{
	mheader_t *h, **hprev;
	mem_pool_t *m;

	m = newh->pool;
#if MEM_DEBUG
	newh->magic = MEMORY_HOLE_MAGIC;
#endif
    m->free_size += newh->size;
	/*
	 * Walk through the hole list and see if this newly freed block can
	 * be merged with anything else to form a larger space.  Whatever
	 * happens, we still ensure that the list is ordered lowest-addressed
	 * -hole first through to highest-addressed-hole last.
	 */
        h = (mheader_t*) m->free_list;
        hprev = (mheader_t**) (void*) &m->free_list;
	for (;;) {
        mem_validate_hole(m, h);
		if (! h) {
			/* At the end of free list */
        		*hprev = newh;
        		NEXT(newh) = 0;
       			break;
        	} else if ((size_t)h > (size_t)newh) {
			/* Insert the new hole before the old one */
        		*hprev = newh;
        		if (((size_t)newh + newh->size) == (size_t)h) {
        			newh->size += h->size;
        			NEXT(newh) = NEXT(h);
        		} else {
	        		NEXT(newh) = h;
        		}
       			break;
		} else if (((size_t)h + h->size) == (size_t)newh) {
			/* Append the new hole at the end of the old one */
        		h->size += newh->size;
        		if (((size_t)h + h->size) == (size_t)NEXT(h)) {
        			h->size += NEXT(h)->size;
        			NEXT(h) = NEXT(NEXT(h));
        		}
       			break;
		}
        	hprev = &NEXT(h);
        	h = NEXT(h);
        }
}

/*
 * Add new hole to the free list.
 */
static void mem_make_hole (mheader_t *newh)
{
	mutex_lock (&newh->pool->lock);
	make_hole_locked (newh);
	mutex_unlock (&newh->pool->lock);
}

/**
 * Release a block of memory.
 */
void mem_free (void *block)
{
	mheader_t *h;

	if (! block)
		return;
#ifdef ARM_CACHED
	/* Clear the non-cached bit. */
	block = (void*) ARM_CACHED (block);
#endif
	/* Make the header pointer. */
	h = H_OF(block);
#if MEM_DEBUG
	if (h->magic != MEMORY_BLOCK_MAGIC) {
		debug_printf ("free[$%x]: bad block[$%x*$%x pool $%x] magic $%2s=$%hx \n"
		        , block, h, h->size, h->pool
		        , &h->magic, h->magic);
		uos_halt(1);
        }
#endif
	/* Convert our block into a hole. */
	mem_make_hole (h);
}

void *mem_realloc (void *old_block, size_t bytes)
{
	mheader_t *h;
	size_t old_size;
	void *block;

	if (! old_block)
		return 0;

	/* Make the header pointer. */
	h = H_OF(old_block);
#ifdef ARM_CACHED
	/* Clear the non-cached bit. */
	h = (mheader_t*) ARM_CACHED (h);
#endif
#if MEM_DEBUG
	if (h->magic != MEMORY_BLOCK_MAGIC) {
		debug_printf ("realloc: bad block magic\n");
		uos_halt(1);
        }
#endif
	old_size = h->size - MEM_HSIZE;
	if (old_size >= bytes)
		return old_block;

	block = mem_alloc (h->pool, bytes);
	if (! block) {
		mem_make_hole (h);
		return 0;
	}
#ifdef ARM_CACHED
	if (ARM_ISNONCACHED (old_block)) {
		/* Set the non-cached bit. */
		block = (void*) ARM_NONCACHED (block);
	}
#endif
	memcpy (block, old_block, old_size);
	mem_make_hole (h);
	return block;
}

void mem_truncate (void *block, size_t required)
{
        mheader_t *h, *newh;

	if (! block)
		return;
#ifdef ARM_CACHED
	/* Clear the non-cached bit. */
	block = (void*) ARM_CACHED (block);
#endif
	/* Add the size of header. */
	if (required < SIZEOF_POINTER)
		required = SIZEOF_POINTER;
	required = MEM_ALIGN (required + MEM_HSIZE);

	/* Make the header pointer. */
	h = H_OF( block );
#if MEM_DEBUG
	if (h->magic != MEMORY_BLOCK_MAGIC) {
		debug_printf ("truncate: bad block magic\n");
		uos_halt(1);
        }
#endif
	/* Is there enough space to split? */
	if (h->size >= required + MEM_HSIZE + 2*SIZEOF_POINTER) {
		/* Split into two blocks. */
		newh = (mheader_t*) ((size_t)h + required);
		newh->pool = h->pool;
		newh->size = h->size - required;
#if MEM_DEBUG
		newh->magic = MEMORY_HOLE_MAGIC;
#endif

		mutex_lock (&newh->pool->lock);
		h->size = required;
		make_hole_locked (newh);
		mutex_unlock (&newh->pool->lock);
	}
}

/**
 * Return the amount of heap space that's still available.
 */
size_t mem_available (mem_pool_t *m)
{
	size_t ret;

	mutex_lock (&m->lock);
	ret = m->free_size;
	mutex_unlock (&m->lock);
	return ret;
}

/*
 * Return the size of the given block.
 */
size_t mem_size (void *block)
{
	mheader_t *h;

	if (! block)
		return 0;

	/* Make the header pointer. */
	h = H_OF(block);
#if MEM_DEBUG
	if (h->magic != MEMORY_BLOCK_MAGIC) {
		debug_printf ("size: bad block magic\n");
		uos_halt(1);
        }
#endif
	return h->size - MEM_HSIZE;
}

/**
 * Return the pool pointer of the given block.
 */
mem_pool_t *mem_pool (void *block)
{
	mheader_t *h;

	if (! block)
		return 0;

	/* Make the header pointer. */
	h = H_OF( block );
#if MEM_DEBUG
	if (h->magic != MEMORY_BLOCK_MAGIC) {
		debug_printf ("pool: bad block magic\n");
		uos_halt(1);
        }
#endif
	return h->pool;
}

#if MEM_DEBUG
void mem_print_free_list (mem_pool_t *m)
{
	mheader_t *h;

	debug_puts ("free list:");
	mutex_lock (&m->lock);
	for (h=(mheader_t*)m->free_list; h; h=NEXT(h)) {
		debug_printf (" %p-%p", h, (char*)h + h->size - 1);
        }
	mutex_unlock (&m->lock);
	debug_puts ("\n");
}

void mem_dump(mem_pool_t *m){
    mheader_t *h;

    //mutex_lock (&m->lock);
    debug_printf ("\npool $%x:", m);
    mheader_t* limit = (mheader_t*)((size_t)m->store+m->size);
    for (h=(mheader_t*)m->store; h<limit; h=SUCC(h)) {
        if (h->pool != m){
            debug_printf ("bad block $%x[$%x]:$%hx on pool[$%x]\n"
                    , h, h->size
                    , h->magic, h->pool);
            break;
        }

        if (h->magic == MEMORY_BLOCK_MAGIC)
            debug_printf ("$%x[$%x] ", h, h->size);
        else if (h->magic == MEMORY_HOLE_MAGIC)
            debug_printf ("$%x[$%x]:->$%x\n", h, h->size, NEXT(h));
        else {
            debug_printf ("$%x[$%x]:bad magic %2s=$%hx\n", h, h->size, &h->magic, (int)(h->magic));
            break;
        }
        if ( (h->size != MEM_ALIGN(h->size))
             || (h->size < (MEM_HSIZE+ SIZEOF_ALIGN))
             || ( h->size > ((size_t)limit - (size_t)h) )
           )
        {
            debug_printf ("bad block $%x size $%x\n", h, h->size);
            break;
        }
    }
    //mutex_unlock (&m->lock);
}

void mem_validate(mem_pool_t *m){
    mheader_t *h;
    //mutex_lock (&m->lock);
    mheader_t* limit = (mheader_t*)((size_t)m->store+m->size);
    for (h=(mheader_t*)m->store; h<limit; h=SUCC(h)) {
        assert2( (h->pool == m)
                , "bad block $%x[$%x]:$%hx on pool[$%x]\n"
                , h, h->size
                , h->magic, h->pool
        );

        assert2( (h->magic == MEMORY_BLOCK_MAGIC) || (h->magic == MEMORY_HOLE_MAGIC)
                , "$%x[$%x]:bad magic %2s=$%hx\n"
                , h, h->size, &h->magic, (int)(h->magic)
                );
        assert2( ( (h->size == MEM_ALIGN(h->size))
                && (h->size >= (MEM_HSIZE+ SIZEOF_ALIGN))
                && (h->size <= ((size_t)limit - (size_t)h) )
                  )
                , "bad block $%x size $%x\n"
                , h, h->size
                );
    }
    //mutex_unlock (&m->lock);
}

void mem_validate_block(void *p){
    mheader_t *h;
    if (! p)
        return;

    /* Make the header pointer. */
    h = H_OF( p );
    assert((size_t)h == MEM_ALIGN(h));
    mem_pool_t *m = h->pool;
    assert(uos_valid_memory_address(m));
    assert( (size_t)h >= (size_t)m->store );
    assert( ((size_t)h + h->size) <= ((size_t)m->store)+m->size );
    assert(h->magic == MEMORY_BLOCK_MAGIC);
           //, "mem block $%x have bad magic\n", p);
}

void mem_validate_hole(mem_pool_t *m, mheader_t *h){
    assert( (size_t)h >= (size_t)m->store );
    assert( ((size_t)h + h->size) <= (((size_t)m->store)+m->size) );
    assert((size_t)h == MEM_ALIGN(h));

    if (h->magic != MEMORY_HOLE_MAGIC) {
        debug_printf ("mem: bad hole magic at 0x%x\n", h);
        debug_printf ("     size=%d, pool=%p\n", h->size, h->pool);
        uos_halt(1);
    }
    if (h->pool != m) {
        debug_printf ("mem: incorect pool pointer=%p, must be %p\n",
            h->pool, m);
        uos_halt(1);
    }
}

#endif

/**
 * Initialize the memory for dynamic allocation.
 * In case of several separate memory chunks,
 * this must be called several times in decreasing start address order.
 *
 * An example: using all memory from bss end to RAM end as dynamic memory.
 *	mem_init (&pool, &__bss_end, END_OF_RAM);
 */
void mem_init (mem_pool_t *m, size_t start, size_t stop)
{
    start = MEM_ALIGN(start);
	mheader_t *h = (mheader_t*) start;

/*debug_printf ("mem_init start=0x%x, size %d bytes\n", start, size);*/
	assert (stop > start);
	h->size = stop - start;
	h->pool = m;
    mutex_lock (&m->lock);
#if MEM_DEBUG
	h->magic = MEMORY_HOLE_MAGIC;
	m->store = h;
    m->size  = h->size;
#endif
	NEXT(h) = (mheader_t *)(m->free_list);
	m->free_list = h;
	m->free_size += h->size;
	mutex_unlock (&m->lock);
}
