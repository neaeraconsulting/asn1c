#include <stdio.h>
#include <assert.h>

/* The library is built without the arena: compile it in here. */
#define ASN_ARENA 1
#include <asn_internal.h>
#include <asn_internal.c>

#define ALIGNED(ptr) (((uintptr_t)(ptr) & 15) == 0)

static int
inside(const void *ptr, const void *buf, size_t size) {
    return (uintptr_t)ptr - (uintptr_t)buf < size;
}

static void
check_without_arena(void) {
    char *p = MALLOC(10);
    char *q = CALLOC(2, 8);
    assert(p && q);
    assert(q[0] == 0 && q[15] == 0);
    p = REALLOC(p, 1000);
    assert(p);
    p[999] = 1;
    FREEMEM(p);
    FREEMEM(q);
    FREEMEM(0);
}

static void
check_buffer(void) {
    char buffer[4096 + 1];
    asn_arena_t arena;
    char *heap = MALLOC(32); /* Allocated before the arena is in use */
    char *a, *b, *c, *first;
    size_t i;

    memset(buffer, 0x55, sizeof(buffer));
    /* A misaligned buffer is fine. */
    asn_arena_init(&arena, buffer + 1, sizeof(buffer) - 1);
    assert(asn_arena_use(&arena) == 0);

    first = a = MALLOC(1);
    assert(inside(a, buffer, sizeof(buffer)) && ALIGNED(a));
    b = CALLOC(3, 7);
    assert(inside(b, buffer, sizeof(buffer)) && ALIGNED(b) && b != a);
    for(i = 0; i < 21; i++) assert(b[i] == 0);
    memset(b, 0x11, 21);

    /* The most recent block grows where it is. */
    c = REALLOC(b, 100);
    assert(c == b);
    for(i = 0; i < 21; i++) assert(c[i] == 0x11);
    /* Shrinking keeps the block. */
    assert(REALLOC(c, 5) == c);

    /* Any other block moves, and keeps its contents. */
    *a = 0x22;
    b = REALLOC(a, 40);
    assert(b != a && inside(b, buffer, sizeof(buffer)) && *b == 0x22);

    /* Freeing the most recent block gives the room back. */
    c = MALLOC(64);
    FREEMEM(c);
    assert(MALLOC(64) == c);
    /* Freeing another block does nothing. */
    FREEMEM(a);
    assert(*b == 0x22);

    /* The memory allocated before is still on the heap. */
    heap = REALLOC(heap, 64);
    assert(heap && !inside(heap, buffer, sizeof(buffer)));
    FREEMEM(heap);

    /* When the buffer is used up, the heap takes over. */
    for(i = 0; i < 100; i++) {
        c = MALLOC(1000);
        assert(c && ALIGNED(c));
        memset(c, 0x33, 1000);
    }
    assert(!inside(c, buffer, sizeof(buffer)));
    assert(arena.chunks != 0);
    /* More than a chunk at once. */
    c = CALLOC(1, 1000000);
    assert(c && c[999999] == 0);
    a = REALLOC(c, 2000000);
    assert(a);
    a[1999999] = 1;
    FREEMEM(a);
    FREEMEM(c);

    /* An absurd size is refused, not wrapped around. */
    assert(MALLOC(~(size_t)0) == 0);
    assert(CALLOC(~(size_t)0 / 2, 4) == 0);

    assert(asn_arena_use(0) == &arena);
    asn_arena_reset(&arena);
    assert(arena.chunks == 0);
    assert(buffer[0] == 0x55);

    /* After the reset the arena starts over. */
    asn_arena_use(&arena);
    assert(MALLOC(1) == first);
    asn_arena_use(0);
    asn_arena_reset(&arena);

    /* No arena in use: back to the heap. */
    a = MALLOC(16);
    assert(a && !inside(a, buffer, sizeof(buffer)));
    FREEMEM(a);
}

static void
check_no_buffer(void) {
    asn_arena_t arena;
    asn_arena_t other;
    char *a, *b;

    asn_arena_init(&arena, 0, 12345);
    asn_arena_init(&other, 0, 0);
    asn_arena_use(&arena);
    a = MALLOC(0);
    b = MALLOC(0);
    assert(a && b && a != b);
    a = REALLOC(0, 24);
    assert(a && ALIGNED(a));

    /* Arenas nest: the one in use before is handed back. */
    assert(asn_arena_use(&other) == &arena);
    b = MALLOC(24);
    assert(b);
    assert(asn_arena_use(&arena) == &other);
    asn_arena_reset(&other);

    asn_arena_use(0);
    asn_arena_reset(&arena);
    asn_arena_reset(&arena); /* Twice is harmless */
}

int
main() {
    check_without_arena();
    check_buffer();
    check_no_buffer();
    printf("OK\n");
    return 0;
}
