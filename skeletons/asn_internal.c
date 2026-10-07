#include <asn_internal.h>

char *
asn__format_umax(char *buf, uintmax_t value, size_t *len) {
    char *end = buf + ASN__FORMAT_INT_SIZE;
    char *p = end;

    do {
        *--p = (char)('0' + (int)(value % 10));
        value /= 10;
    } while(value);

    *len = end - p;
    return p;
}

char *
asn__format_imax(char *buf, intmax_t value, size_t *len) {
    char *p;

    if(value >= 0) return asn__format_umax(buf, (uintmax_t)value, len);

    /* Negate as unsigned, the most negative value has no positive twin. */
    p = asn__format_umax(buf, 0 - (uintmax_t)value, len);
    *--p = '-';
    ++*len;
    return p;
}

int
asn__callback3(int (*cb)(const void *, size_t, void *key), void *key,
               const void *buf1, size_t size1, const void *buf2, size_t size2,
               const void *buf3, size_t size3) {
    char scratch[128];

    if(size1 < sizeof(scratch) && size2 < sizeof(scratch)
       && size3 < sizeof(scratch)
       && size1 + size2 + size3 <= sizeof(scratch)) {
        char *p = scratch;
        if(size1) { memcpy(p, buf1, size1); p += size1; }
        if(size2) { memcpy(p, buf2, size2); p += size2; }
        if(size3) { memcpy(p, buf3, size3); p += size3; }
        return (cb(scratch, p - scratch, key) < 0) ? -1 : 0;
    }

    if(cb(buf1, size1, key) < 0 || cb(buf2, size2, key) < 0
       || (size3 && cb(buf3, size3, key) < 0))
        return -1;
    return 0;
}

ssize_t
asn__text_indent(int (*cb)(const void *, size_t, void *key), void *key, int nl,
                 int level) {
    /* A newline followed by 16 levels of indentation. */
    static const char text[] =
        "\n"
        "                                "
        "                                ";
    const int max_level = (int)(sizeof(text) - 2) / 4;
    ssize_t wrote = 0;

    nl = (nl != 0);
    if(level < 0) level = 0;

    if(level <= max_level) {
        size_t size = nl + 4 * (size_t)level;
        if(size && cb(text + 1 - nl, size, key) < 0) return -1;
        return size;
    }

    if(nl) {
        if(cb(text, 1, key) < 0) return -1;
        wrote = 1;
    }
    while(level > 0) {
        int chunk = level < max_level ? level : max_level;
        if(cb(text + 1, 4 * (size_t)chunk, key) < 0) return -1;
        wrote += 4 * chunk;
        level -= chunk;
    }
    return wrote;
}

ssize_t
asn__format_to_callback(int (*cb)(const void *, size_t, void *key), void *key,
                        const char *fmt, ...) {
    char scratch[64];
    char *buf = scratch;
    size_t buf_size = sizeof(scratch);
    int wrote;
    int cb_ret;

    do {
        va_list args;
        va_start(args, fmt);

        wrote = vsnprintf(buf, buf_size, fmt, args);
        va_end(args);
        if(wrote < (ssize_t)buf_size) {
            if(wrote < 0) {
                if(buf != scratch) FREEMEM(buf);
                return -1;
            }
            break;
        }

        buf_size <<= 1;
        if(buf == scratch) {
            buf = MALLOC(buf_size);
            if(!buf) {
              return -1;
            }
        } else {
            void *p = REALLOC(buf, buf_size);
            if(!p) {
                FREEMEM(buf);
                return -1;
            }
            buf = p;
        }
    } while(1);

    cb_ret = cb(buf, wrote, key);
    if(buf != scratch) FREEMEM(buf);
    if(cb_ret < 0) {
        return -1;
    }

    return wrote;
}


#ifdef ASN_ARENA

#ifdef _MSC_VER
#define ASN__THREAD_LOCAL __declspec(thread)
#else
#define ASN__THREAD_LOCAL __thread
#endif

#define ASN__ARENA_ALIGN 16
#define ASN__ARENA_ROUND(size) \
    (((size) + (ASN__ARENA_ALIGN - 1)) & ~(size_t)(ASN__ARENA_ALIGN - 1))
#define ASN__ARENA_CHUNK_SIZE 65536 /* The least to take from the heap */

/* Memory taken from the heap, the blocks follow the header. */
typedef struct asn_arena_chunk_s {
    struct asn_arena_chunk_s *next;
    size_t size; /* Including this header */
} asn_arena_chunk_t;
#define ASN__ARENA_CHUNK_HEADER ASN__ARENA_ROUND(sizeof(asn_arena_chunk_t))

/* Precedes every block, so that it can be reallocated. */
typedef union {
    size_t size; /* Size of the block, rounded */
    char align[ASN__ARENA_ALIGN];
} asn_arena_block_t;
#define ASN__ARENA_BLOCK(ptr) \
    ((asn_arena_block_t *)(void *)((char *)(ptr) - sizeof(asn_arena_block_t)))

static ASN__THREAD_LOCAL asn_arena_t *asn__arena; /* In use by the thread */

static void
asn__arena_rewind(asn_arena_t *arena) {
    size_t skip = 0;

    if(arena->buffer) {
        skip = (size_t)(0 - (uintptr_t)arena->buffer) & (ASN__ARENA_ALIGN - 1);
        if(skip > arena->buffer_size) skip = arena->buffer_size;
    }
    arena->next_free = arena->buffer ? arena->buffer + skip : 0;
    arena->available = arena->buffer_size - skip;
    arena->last_block = 0;
}

void
asn_arena_init(asn_arena_t *arena, void *buffer, size_t size) {
    arena->buffer = (char *)buffer;
    arena->buffer_size = buffer ? size : 0;
    arena->chunks = 0;
    asn__arena_rewind(arena);
}

asn_arena_t *
asn_arena_use(asn_arena_t *arena) {
    asn_arena_t *previous = asn__arena;
    asn__arena = arena;
    return previous;
}

void
asn_arena_reset(asn_arena_t *arena) {
    while(arena->chunks) {
        asn_arena_chunk_t *chunk = arena->chunks;
        arena->chunks = chunk->next;
        free(chunk);
    }
    asn__arena_rewind(arena);
}

static int
asn__arena_owns(const asn_arena_t *arena, const void *ptr) {
    uintptr_t p = (uintptr_t)ptr;
    const asn_arena_chunk_t *chunk;

    if(p - (uintptr_t)arena->buffer < arena->buffer_size) return 1;
    for(chunk = arena->chunks; chunk; chunk = chunk->next) {
        if(p - (uintptr_t)chunk < chunk->size) return 1;
    }
    return 0;
}

static void *
asn__arena_alloc(asn_arena_t *arena, size_t size) {
    asn_arena_block_t *block;
    size_t need;

    if(size > (~(size_t)0 >> 1)) return 0;
    size = ASN__ARENA_ROUND(size);
    need = sizeof(*block) + size;

    if(arena->available < need) {
        asn_arena_chunk_t *chunk;
        size_t chunk_size = ASN__ARENA_CHUNK_HEADER + need;
        if(chunk_size < ASN__ARENA_CHUNK_SIZE)
            chunk_size = ASN__ARENA_CHUNK_SIZE;
        chunk = (asn_arena_chunk_t *)malloc(chunk_size);
        if(!chunk) return 0;
        chunk->next = arena->chunks;
        chunk->size = chunk_size;
        arena->chunks = chunk;
        arena->next_free = (char *)chunk + ASN__ARENA_CHUNK_HEADER;
        arena->available = chunk_size - ASN__ARENA_CHUNK_HEADER;
    }

    block = (asn_arena_block_t *)(void *)arena->next_free;
    block->size = size;
    arena->last_block = (char *)(block + 1);
    arena->next_free += need;
    arena->available -= need;
    return arena->last_block;
}

void *
asn__arena_malloc(size_t size) {
    asn_arena_t *arena = asn__arena;
    return arena ? asn__arena_alloc(arena, size) : malloc(size);
}

void *
asn__arena_calloc(size_t nmemb, size_t size) {
    asn_arena_t *arena = asn__arena;
    void *ptr;

    if(!arena) return calloc(nmemb, size);

    if(size && nmemb > (~(size_t)0 >> 1) / size) return 0;
    ptr = asn__arena_alloc(arena, nmemb * size);
    if(ptr) memset(ptr, 0, nmemb * size);
    return ptr;
}

void *
asn__arena_realloc(void *oldptr, size_t size) {
    asn_arena_t *arena = asn__arena;
    size_t old_size;
    void *ptr;

    if(!arena || (oldptr && !asn__arena_owns(arena, oldptr)))
        return realloc(oldptr, size);
    if(!oldptr) return asn__arena_alloc(arena, size);

    old_size = ASN__ARENA_BLOCK(oldptr)->size;
    if(size <= old_size) return oldptr;

    /* The most recent block grows in place, if there is room. */
    if(oldptr == arena->last_block && size <= (~(size_t)0 >> 1)) {
        size_t more = ASN__ARENA_ROUND(size) - old_size;
        if(more <= arena->available) {
            ASN__ARENA_BLOCK(oldptr)->size += more;
            arena->next_free += more;
            arena->available -= more;
            return oldptr;
        }
    }

    ptr = asn__arena_alloc(arena, size);
    if(ptr) memcpy(ptr, oldptr, old_size);
    return ptr;
}

void
asn__arena_free(void *ptr) {
    asn_arena_t *arena = asn__arena;

    if(!ptr) return;
    if(!arena || !asn__arena_owns(arena, ptr)) {
        free(ptr);
        return;
    }

    /* The most recent block is given back, the others stay until reset. */
    if(ptr == arena->last_block) {
        size_t size = sizeof(asn_arena_block_t) + ASN__ARENA_BLOCK(ptr)->size;
        arena->next_free -= size;
        arena->available += size;
        arena->last_block = 0;
    }
}

#endif /* ASN_ARENA */
