/*
 * Copyright (c) 2017 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <NativeInteger.h>

/*
 * Stands for "no constraints" where passing a null pointer would make
 * the INTEGER codec fall back to the constraints of the type.
 */
static const asn_per_constraints_t asn_PER_unconstrained = {
    {APC_UNCONSTRAINED, -1, -1, 0, 0},
    {APC_UNCONSTRAINED, -1, -1, 0, 0},
    0, 0
};

asn_dec_rval_t
NativeInteger_decode_uper(const asn_codec_ctx_t *opt_codec_ctx,
                          const asn_TYPE_descriptor_t *td,
                          const asn_per_constraints_t *constraints, void **sptr,
                          asn_per_data_t *pd) {
    const asn_INTEGER_specifics_t *specs =
        (const asn_INTEGER_specifics_t *)td->specifics;
    asn_dec_rval_t rval;
    long *native = (long *)*sptr;
    const asn_per_constraint_t *ct;
    asn_per_constraints_t noext;
    INTEGER_t tmpint;
    void *tmpintptr = &tmpint;

    (void)opt_codec_ctx;
    ASN_DEBUG("Decoding NativeInteger %s (UPER)", td->name);

    if(!native) {
        native = (long *)(*sptr = CALLOC(1, sizeof(*native)));
        if(!native) ASN__DECODE_FAILED;
    }

    if(!constraints) constraints = td->encoding_constraints.per_constraints;
    ct = constraints ? &constraints->value : 0;

    if(ct && ct->flags & APC_EXTENSIBLE) {
        int inext = per_get_few_bits(pd, 1);
        if(inext < 0) ASN__DECODE_STARVED;
        /* The extension bit is consumed, don't let INTEGER look for it. */
        if(inext) {
            constraints = &asn_PER_unconstrained;
            ct = 0;
        } else {
            noext = *constraints;
            noext.value.flags &= ~APC_EXTENSIBLE;
            constraints = &noext;
        }
    }

    /*
     * X.691-2008/11, #13.2.2, constrained whole number.
     * Decode it straight into the native value;
     * everything else goes through the temporary INTEGER.
     */
    if(ct && ct->flags != APC_UNCONSTRAINED && ct->range_bits >= 0) {
        uintmax_t uvalue = 0;

        if((size_t)ct->range_bits > 8 * sizeof(uintmax_t))
            ASN__DECODE_FAILED;
        if(uper_get_constrained_whole_number(pd, &uvalue, ct->range_bits))
            ASN__DECODE_STARVED;

        if(specs && specs->field_unsigned) {
            uvalue += ct->lower_bound;
            if(uvalue > (uintmax_t)ct->upper_bound || uvalue > ULONG_MAX)
                ASN__DECODE_FAILED;
            *(unsigned long *)native = (unsigned long)uvalue;
        } else {
            intmax_t svalue;
            if(per_imax_range_unrebase(uvalue, ct->lower_bound,
                                       ct->upper_bound, &svalue)
               || svalue < LONG_MIN || svalue > LONG_MAX)
                ASN__DECODE_FAILED;
            *native = (long)svalue;
        }
        ASN_DEBUG("NativeInteger %s got value %ld", td->name, *native);

        rval.code = RC_OK;
        rval.consumed = 0;
        return rval;
    }

    memset(&tmpint, 0, sizeof tmpint);
    rval = INTEGER_decode_uper(opt_codec_ctx, td, constraints,
                               &tmpintptr, pd);
    if(rval.code == RC_OK) {
        if((specs&&specs->field_unsigned)
            ? asn_INTEGER2ulong(&tmpint, (unsigned long *)native)
            : asn_INTEGER2long(&tmpint, native))
            rval.code = RC_FAIL;
        else
            ASN_DEBUG("NativeInteger %s got value %ld",
                      td->name, *native);
    }
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_INTEGER, &tmpint);

    return rval;
}

asn_enc_rval_t
NativeInteger_encode_uper(const asn_TYPE_descriptor_t *td,
                          const asn_per_constraints_t *constraints,
                          const void *sptr, asn_per_outp_t *po) {
    const asn_INTEGER_specifics_t *specs =
        (const asn_INTEGER_specifics_t *)td->specifics;
    asn_enc_rval_t er = {0,0,0};
    const asn_per_constraint_t *ct;
    long native;
    INTEGER_t tmpint;

    if(!sptr) ASN__ENCODE_FAILED;

    native = *(const long *)sptr;

    ASN_DEBUG("Encoding NativeInteger %s %ld (UPER)", td->name, native);

    /*
     * X.691-11/2008, #13.2.2, constrained whole number within its range.
     * Encode it straight from the native value;
     * everything else goes through the temporary INTEGER.
     */
    ct = constraints ? &constraints->value
         : td->encoding_constraints.per_constraints
               ? &td->encoding_constraints.per_constraints->value : 0;
    if(ct && ct->range_bits >= 0 && !(ct->flags & APC_SEMI_CONSTRAINED)) {
        uintmax_t v;
        int in_range;

        if(specs && specs->field_unsigned) {
            uintmax_t uvalue = (unsigned long)native;
            in_range = (uvalue >= (uintmax_t)ct->lower_bound
                        && uvalue <= (uintmax_t)ct->upper_bound);
            v = uvalue - (uintmax_t)ct->lower_bound;
        } else {
            in_range = (native >= ct->lower_bound && native <= ct->upper_bound
                        && per_imax_range_rebase(native, ct->lower_bound,
                                                 ct->upper_bound, &v) == 0);
        }

        if(in_range) {
            if((ct->flags & APC_EXTENSIBLE) && per_put_few_bits(po, 0, 1))
                ASN__ENCODE_FAILED;
            if(uper_put_constrained_whole_number_u(po, v, ct->range_bits))
                ASN__ENCODE_FAILED;
            ASN__ENCODED_OK(er);
        }
    }

    memset(&tmpint, 0, sizeof(tmpint));
    if((specs&&specs->field_unsigned)
        ? asn_ulong2INTEGER(&tmpint, native)
        : asn_long2INTEGER(&tmpint, native))
        ASN__ENCODE_FAILED;
    er = INTEGER_encode_uper(td, constraints, &tmpint, po);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_INTEGER, &tmpint);
    return er;
}
