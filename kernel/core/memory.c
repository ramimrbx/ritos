#include <kernel/memory.h>

// Defined by linker script
extern uint8_t end[];

/*
 * Free-list heap allocator. Every allocation is preceded by a block_t
 * header; all blocks (used and free) form a doubly-linked list in address
 * order, so freeing can coalesce with both physical neighbours and freed
 * memory is reused by later allocations.
 */

#define HEAP_MAX    0x4000000  /* 64MB limit */
#define MAGIC_USED  0xA110C8ED
#define MAGIC_FREE  0xF7EEB10C
#define MIN_SPLIT   32         /* don't split off slivers smaller than this */

typedef struct block {
	uint32_t magic;        /* MAGIC_USED or MAGIC_FREE */
	uint32_t size;         /* payload bytes (16-aligned) */
	struct block* prev;    /* physically previous block */
	struct block* next;    /* physically next block */
} block_t;

static uint8_t* heap_start = NULL;
static uint8_t* heap_brk   = NULL;   /* first byte past the last block */
static block_t* heap_head  = NULL;
static block_t* heap_tail  = NULL;
static size_t   used_bytes = 0;

void heap_init(void) {
	// Idempotent: a second call must not reset the heap,
	// or earlier allocations would be handed out again.
	if (heap_start != NULL) return;
	heap_start = (uint8_t*)(((uintptr_t)end + 4095) & ~4095);
	heap_brk   = heap_start;
	heap_head  = NULL;
	heap_tail  = NULL;
	used_bytes = 0;
}

void* kmalloc(size_t size) {
	if (heap_start == NULL) {
		heap_init();
	}
	if (size == 0) size = 1;
	size = (size + 15) & ~15;

	/* First fit: reuse a freed block if one is big enough */
	for (block_t* b = heap_head; b; b = b->next) {
		if (b->magic != MAGIC_FREE || b->size < size) continue;

		/* Split off the unused tail if it is worth a new block */
		if (b->size >= size + sizeof(block_t) + MIN_SPLIT) {
			block_t* nb = (block_t*)((uint8_t*)(b + 1) + size);
			nb->magic = MAGIC_FREE;
			nb->size  = b->size - size - sizeof(block_t);
			nb->prev  = b;
			nb->next  = b->next;
			if (b->next) b->next->prev = nb; else heap_tail = nb;
			b->next = nb;
			b->size = size;
		}
		b->magic = MAGIC_USED;
		used_bytes += b->size;
		/* Recycled memory contains old data; hand out zeroed memory like
		 * fresh pages, since early callers grew up relying on that. */
		uint8_t* p = (uint8_t*)(b + 1);
		for (size_t i = 0; i < b->size; i++) p[i] = 0;
		return p;
	}

	/* No fit: extend the heap with a new block */
	if ((uintptr_t)(heap_brk + sizeof(block_t) + size) >
	    (uintptr_t)(heap_start + HEAP_MAX)) {
		return NULL; /* Out of memory */
	}
	block_t* nb = (block_t*)heap_brk;
	nb->magic = MAGIC_USED;
	nb->size  = size;
	nb->prev  = heap_tail;
	nb->next  = NULL;
	if (heap_tail) heap_tail->next = nb; else heap_head = nb;
	heap_tail = nb;
	heap_brk  = (uint8_t*)(nb + 1) + size;
	used_bytes += size;
	return (void*)(nb + 1);
}

void kfree(void* ptr) {
	if (ptr == NULL) return;
	block_t* b = ((block_t*)ptr) - 1;
	if (b->magic != MAGIC_USED) return; /* invalid or double free: ignore */

	b->magic = MAGIC_FREE;
	used_bytes -= b->size;

	/* Coalesce with the next block if it is free */
	if (b->next && b->next->magic == MAGIC_FREE) {
		block_t* n = b->next;
		b->size += sizeof(block_t) + n->size;
		b->next  = n->next;
		if (n->next) n->next->prev = b; else heap_tail = b;
		n->magic = 0;
	}
	/* Coalesce with the previous block if it is free */
	if (b->prev && b->prev->magic == MAGIC_FREE) {
		block_t* p = b->prev;
		p->size += sizeof(block_t) + b->size;
		p->next  = b->next;
		if (b->next) b->next->prev = p; else heap_tail = p;
		b->magic = 0;
	}
}

size_t get_heap_usage(void) {
	return used_bytes;
}
