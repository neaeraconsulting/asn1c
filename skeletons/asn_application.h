/*-
 * Copyright (c) 2004-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
/*
 * Application-level ASN.1 callbacks.
 */
#ifndef	ASN_APPLICATION_H
#define	ASN_APPLICATION_H

#include "asn_system.h"		/* for platform-dependent types */
#include "asn_codecs.h"		/* for ASN.1 codecs specifics */
#include "asn_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A selection of ASN.1 Transfer Syntaxes to use with generalized
 * encoders and decoders declared further in this .h file.
 */
enum asn_transfer_syntax {
    /* Avoid appearance of a default transfer syntax. */
    ATS_INVALID = 0,
    /* Plaintext output (not conforming to any standard), for debugging. */
    ATS_NONSTANDARD_PLAINTEXT,
    /* Returns a randomly generated structure. */
    ATS_RANDOM,
    /*
     * X.690:
     * BER: Basic Encoding Rules.
     * DER: Distinguished Encoding Rules.
     * CER: Canonical Encoding Rules.
     * DER and CER are more strict variants of BER.
     */
    ATS_BER,
    ATS_DER,
    ATS_CER, /* Only decoding is supported */
    /*
     * X.696:
     * OER: Octet Encoding Rules.
     * CANONICAL-OER is a more strict variant of BASIC-OER.
     */
    ATS_BASIC_OER,
    ATS_CANONICAL_OER,
    /*
     * X.691:
     * PER: Packed Encoding Rules.
     * CANONICAL-PER is a more strict variant of BASIC-PER.
     * NOTE: Produces or consumes a complete encoding (X.691 (08/2015) #11.1).
     */
    ATS_UNALIGNED_BASIC_PER,
    ATS_UNALIGNED_CANONICAL_PER,
    ATS_ALIGNED_BASIC_PER,
    ATS_ALIGNED_CANONICAL_PER,
    /*
     * X.693:
     * XER: XML Encoding Rules.
     * CANONICAL-XER is a more strict variant of BASIC-XER.
     */
    ATS_BASIC_XER,
    ATS_CANONICAL_XER,
    /*
     * X.697:
     * JER: JSON Encoding Rules.
     * MINIFIED produces a whitespace-free JSON.
     */
    ATS_JER,
    ATS_JER_MINIFIED,
};

/*
 * A generic encoder for any supported transfer syntax.
 * RETURN VALUES:
 * The (.encoded) field of the return value is REDEFINED to mean the following:
 * >=0: The computed size of the encoded data. Can exceed the (buffer_size).
 *  -1: Error encoding the structure. See the error code in (errno):
 *      EINVAL: Incorrect parameters to the function, such as NULLs.
 *      ENOENT: Encoding transfer syntax is not defined (for this type).
 *      EBADF:  The structure has invalid form or content constraint failed.
 *      The (.failed_type) and (.structure_ptr) MIGHT be set to the appropriate
 *      values at the place of failure, if at all possible.
 * WARNING: The (.encoded) field of the return value can exceed the buffer_size.
 * This is similar to snprintf(3) contract which might return values
 * greater than the buffer size.
 */
asn_enc_rval_t asn_encode_to_buffer(
    const asn_codec_ctx_t *opt_codec_parameters, /* See asn_codecs.h */
    enum asn_transfer_syntax,
    const struct asn_TYPE_descriptor_s *type_to_encode,
    const void *structure_to_encode, void *buffer, size_t buffer_size);

/*
 * A variant of asn_encode_to_buffer() with automatically allocated buffer.
 * RETURN VALUES:
 * On success, returns a newly allocated (.buffer) containing the whole message.
 * The message size is returned in (.result.encoded).
 * On failure:
 *  (.buffer) is NULL,
 *  (.result.encoded) as in asn_encode_to_buffer(),
 *  The errno codes as in asn_encode_to_buffer(), plus the following:
 *      ENOMEM: Memory allocation failed due to system or internal limits.
 * The user is responsible for freeing the (.buffer).
 */
typedef struct asn_encode_to_new_buffer_result_s {
    void *buffer;   /* NULL if failed to encode. */
    asn_enc_rval_t result;
} asn_encode_to_new_buffer_result_t;
asn_encode_to_new_buffer_result_t asn_encode_to_new_buffer(
    const asn_codec_ctx_t *opt_codec_parameters, /* See asn_codecs.h */
    enum asn_transfer_syntax,
    const struct asn_TYPE_descriptor_s *type_to_encode,
    const void *structure_to_encode);


/*
 * Generic type of an application-defined callback to return various
 * types of data to the application.
 * EXPECTED RETURN VALUES:
 *  -1: Failed to consume bytes. Abort the mission.
 * Non-negative return values indicate success, and ignored.
 */
typedef int(asn_app_consume_bytes_f)(const void *buffer, size_t size,
                                     void *application_specific_key);


/*
 * A generic encoder for any supported transfer syntax.
 * Returns the comprehensive encoding result descriptor (see asn_codecs.h).
 * RETURN VALUES:
 * The negative (.encoded) field of the return values is accompanied with the
 * following error codes (errno):
 *      EINVAL: Incorrect parameters to the function, such as NULLs.
 *      ENOENT: Encoding transfer syntax is not defined (for this type).
 *      EBADF:  The structure has invalid form or content constraint failed.
 *      EIO:    The (callback) has returned negative value during encoding.
 */
asn_enc_rval_t asn_encode(
    const asn_codec_ctx_t *opt_codec_parameters, /* See asn_codecs.h */
    enum asn_transfer_syntax,
    const struct asn_TYPE_descriptor_s *type_to_encode,
    const void *structure_to_encode,
    asn_app_consume_bytes_f *callback, void *callback_key);


/*
 * A generic decoder for any supported transfer syntax.
 */
asn_dec_rval_t asn_decode(
    const asn_codec_ctx_t *opt_codec_parameters, enum asn_transfer_syntax,
    const struct asn_TYPE_descriptor_s *type_to_decode,
    void **structure_ptr, /* Pointer to a target structure's pointer */
    const void *buffer,   /* Data to be decoded */
    size_t size           /* Size of that buffer */
);


#ifdef ASN_ARENA
/*
 * Arena allocation, available when the codec is compiled with -DASN_ARENA.
 *
 * While a thread uses an arena, whatever the codec allocates on that thread
 * (decoded structures as well as the temporary buffers of the encoders)
 * is carved out of the arena, and freeing it does nothing. All of it is
 * released at once by asn_arena_reset(). This suits the applications which
 * decode a message, look at it or encode it again, and drop it:
 *
 *      char buffer[65536];
 *      asn_arena_t arena;
 *      asn_arena_init(&arena, buffer, sizeof(buffer));
 *      asn_arena_use(&arena);
 *      (decode, check constraints, encode)
 *      asn_arena_use(0);
 *      asn_arena_reset(&arena);
 *
 * The arena starts with the memory given to asn_arena_init(), which may be
 * on the stack, and takes more from the heap when that is used up.
 * RULES:
 *  - A structure allocated from an arena dies with asn_arena_reset().
 *    ASN_STRUCT_FREE() is not necessary for it. Calling it is harmless as
 *    long as the same arena is still in use by the calling thread, and
 *    is an error after that.
 *  - The memory allocated before asn_arena_use() is freed as usual,
 *    whether an arena is in use or not.
 *  - An arena is used by one thread at a time.
 *  - The buffer returned by asn_encode_to_new_buffer() never comes from
 *    an arena and has to be freed by the application, as always.
 */
typedef struct asn_arena_s {
    /* The contents are private to the arena functions. */
    char *buffer;       /* Memory given to asn_arena_init() */
    size_t buffer_size;
    char *next_free;    /* Where the next allocation goes */
    size_t available;   /* Bytes left after (next_free) */
    char *last_block;   /* The most recent allocation */
    struct asn_arena_chunk_s *chunks; /* Memory taken from the heap */
} asn_arena_t;

/*
 * Prepare an arena which starts with the given memory.
 * The (buffer) may be NULL: the arena then takes everything from the heap.
 */
void asn_arena_init(asn_arena_t *arena, void *buffer, size_t size);

/*
 * Make the calling thread allocate from the given arena, or from the heap
 * if (arena) is NULL. Returns the arena which was in use before.
 */
asn_arena_t *asn_arena_use(asn_arena_t *arena);

/*
 * Release everything allocated from the arena and return to the heap
 * the memory which the arena has taken from it. The arena is ready
 * to be used again.
 */
void asn_arena_reset(asn_arena_t *arena);
#endif  /* ASN_ARENA */

/*
 * A callback of this type is called whenever constraint validation fails
 * on some ASN.1 type. See "constraints.h" for more details on constraint
 * validation.
 * This callback specifies a descriptor of the ASN.1 type which failed
 * the constraint check, as well as human readable message on what
 * particular constraint has failed.
 */
typedef void (asn_app_constraint_failed_f)(void *application_specific_key,
	const struct asn_TYPE_descriptor_s *type_descriptor_which_failed,
	const void *structure_which_failed_ptr,
	const char *error_message_format, ...) CC_PRINTFLIKE(4, 5);


#ifdef __cplusplus
}
#endif

#include "constr_TYPE.h"	/* for asn_TYPE_descriptor_t */

#endif	/* ASN_APPLICATION_H */
