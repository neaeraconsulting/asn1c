/*
 * Copyright (c) 2017 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <NativeInteger.h>

/*
 * Decode the chunk of JSON text encoding INTEGER.
 */
asn_dec_rval_t
NativeInteger_decode_jer(const asn_codec_ctx_t *opt_codec_ctx,
                         const asn_TYPE_descriptor_t *td,
                         const asn_jer_constraints_t* constraints,
                         void **sptr, const void *buf_ptr,
                         size_t size) {
    const asn_INTEGER_specifics_t *specs =
        (const asn_INTEGER_specifics_t *)td->specifics;
    asn_dec_rval_t rval;
    INTEGER__text_value_t text;
    long *native = (long *)*sptr;

    if(!native) {
        native = (long *)(*sptr = CALLOC(1, sizeof(*native)));
        if(!native) ASN__DECODE_FAILED;
    }

    (void)constraints;
    rval = INTEGER__decode_jer_value(opt_codec_ctx, td, &text, buf_ptr, size);
    if(rval.code == RC_OK) {
        long l;
        if(text.has_value
           && (text.value >= 0 || !(specs && specs->field_unsigned))) {
            /* A plain number: no need for the INTEGER representation. */
            if((specs && specs->field_unsigned)
                   ? ((uintmax_t)text.value > ULONG_MAX)
                   : (text.value < LONG_MIN || text.value > LONG_MAX)) {
                rval.code = RC_FAIL;
                rval.consumed = 0;
            } else {
                *native = (long)text.value;
            }
        } else if((text.has_value && asn_imax2INTEGER(&text.st, text.value))
                  || ((specs&&specs->field_unsigned)
                      ? asn_INTEGER2ulong(&text.st, (unsigned long *)&l) /* sic */
                      : asn_INTEGER2long(&text.st, &l))) {
            rval.code = RC_FAIL;
            rval.consumed = 0;
        } else {
            *native = l;
        }
    } else {
        /*
         * Cannot restart from the middle;
         * there is no place to save state in the native type.
         * Request a continuation from the very beginning.
         */
        rval.consumed = 0;
    }
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_INTEGER, &text.st);
    return rval;
}

asn_enc_rval_t
NativeInteger_encode_jer(const asn_TYPE_descriptor_t *td,
                         const asn_jer_constraints_t* constraints,
                         const void *sptr, int ilevel,
                         enum jer_encoder_flags_e flags,
                         asn_app_consume_bytes_f *cb, void *app_key) {
    const asn_INTEGER_specifics_t *specs =
        (const asn_INTEGER_specifics_t *)td->specifics;
    char scratch[ASN__FORMAT_INT_SIZE];
    asn_enc_rval_t er = {0,0,0};
    const long *native = (const long *)sptr;
    const char *text;
    size_t text_len;

    (void)ilevel;
    (void)flags;

    if(!native) ASN__ENCODE_FAILED;

    text = (specs && specs->field_unsigned)
               ? asn__format_umax(scratch, (unsigned long)*native, &text_len)
               : asn__format_imax(scratch, *native, &text_len);
    if(cb(text, text_len, app_key) < 0)
        ASN__ENCODE_FAILED;
    er.encoded = text_len;

    ASN__ENCODED_OK(er);
}
