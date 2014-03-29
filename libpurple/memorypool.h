/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef PURPLE_MEMORY_POOL_H
#define PURPLE_MEMORY_POOL_H

#include <glib-object.h>

#define PURPLE_TYPE_MEMORY_POOL (purple_memory_pool_get_type())
#define PURPLE_MEMORY_POOL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEMORY_POOL, PurpleMemoryPool))
#define PURPLE_MEMORY_POOL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEMORY_POOL, PurpleMemoryPoolClass))
#define PURPLE_IS_MEMORY_POOL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEMORY_POOL))
#define PURPLE_IS_MEMORY_POOL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEMORY_POOL))
#define PURPLE_MEMORY_POOL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEMORY_POOL, PurpleMemoryPoolClass))

typedef struct _PurpleMemoryPool PurpleMemoryPool;
typedef struct _PurpleMemoryPoolClass PurpleMemoryPoolClass;

struct _PurpleMemoryPool
{
	/*< private >*/
	GObject parent_instance;
};

struct _PurpleMemoryPoolClass
{
	/*< private >*/
	GObjectClass parent_class;

	gpointer (*palloc)(PurpleMemoryPool *pool, gsize size, guint alignment);
	gpointer (*pfree)(PurpleMemoryPool *pool, gpointer mem);
	void (*cleanup)(PurpleMemoryPool *pool);

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

GType
purple_memory_pool_get_type(void);

/**
 * purple_memory_pool_new:
 *
 * Creates a new memory pool.
 *
 * Returns: The new #PurpleMemoryPool.
 */
PurpleMemoryPool *
purple_memory_pool_new(void);

/**
 * purple_memory_pool_set_block_size:
 * @pool: The memory pool.
 * @block_size: The new default block size.
 *
 * Sets new default block size for a memory pool. You might want to call this
 * before any allocation, to have it applied to the every created block.
 */
void
purple_memory_pool_set_block_size(PurpleMemoryPool *pool, gulong block_size);

/**
 * purple_memory_pool_alloc:
 * @pool: The memory pool.
 * @size: The size of memory to be allocated.
 * @alignment: The alignment of memory block (should be a power of two).
 *
 * Allocates an aligned memory block within a pool.
 *
 * Returns: the pointer to a memory block. This should be freed with
 *          a call to purple_memory_pool_free.
 */
gpointer
purple_memory_pool_alloc(PurpleMemoryPool *pool, gsize size, guint alignment);

/**
 * purple_memory_pool_alloc0:
 * @pool: The memory pool.
 * @size: The size of memory to be allocated.
 * @alignment: The alignment of memory block (should be a power of two).
 *
 * Allocates an aligned memory block within a pool and sets its contents to
 * zeros.
 *
 * Returns: the pointer to a memory block. This should be freed with
 *          a call to purple_memory_pool_free.
 */
gpointer
purple_memory_pool_alloc0(PurpleMemoryPool *pool, gsize size, guint alignment);

/**
 * purple_memory_pool_free:
 * @pool: The memory pool.
 * @mem: The pointer to a memory block.
 *
 * Frees a memory allocated within a memory pool. This can be a no-op in certain
 * implementations. Thus, it don't need to be called in every case.
 */
void
purple_memory_pool_free(PurpleMemoryPool *pool, gpointer mem);

/**
 * purple_memory_pool_cleanup:
 * @pool: The memory pool.
 *
 * Marks all memory allocated within a memory pool as not used. It may free
 * resources, but don't have to.
 */
void
purple_memory_pool_cleanup(PurpleMemoryPool *pool);

/**
 * purple_memory_pool_strdup:
 * @pool: The memory pool.
 * @str: The string to duplicate.
 *
 * Duplicates a string using a memory allocated within a memory pool. If @str is
 * %NULL it returns %NULL. The returned string should be freed with g_free()
 * when no longer needed.
 *
 * Returns: a newly-allocated copy of @str
 */
gchar *
purple_memory_pool_strdup(PurpleMemoryPool *pool, const gchar *str);

G_END_DECLS

#endif /* PURPLE_MEMORY_POOL_H */
