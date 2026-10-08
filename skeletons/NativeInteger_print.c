/*
 * Copyright (c) 2017 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <NativeInteger.h>

/*
 * INTEGER specific human-readable output.
 */
int
NativeInteger_print(const asn_TYPE_descriptor_t *td, const void *sptr,
                    int ilevel, asn_app_consume_bytes_f *cb, void *app_key) {
    const asn_INTEGER_specifics_t *specs =
        (const asn_INTEGER_specifics_t *)td->specifics;
    const long *native = (const long *)sptr;
    char scratch[ASN__FORMAT_INT_SIZE];

    (void)td;      /* Unused argument */
    (void)ilevel;  /* Unused argument */

    if(native) {
        long value = *native;
        size_t text_len;
        const char *text =
            (specs && specs->field_unsigned)
                ? asn__format_umax(scratch, (unsigned long)value, &text_len)
                : asn__format_imax(scratch, value, &text_len);
        if(cb(text, text_len, app_key) < 0) return -1;
        if(specs && (value >= 0 || !specs->field_unsigned)) {
            const asn_INTEGER_enum_map_t *el =
                INTEGER_map_value2enum(specs, value);
            if(el) {
                if(cb(" (", 2, app_key) < 0) return -1;
                if(cb(el->enum_name, el->enum_len, app_key) < 0) return -1;
                if(cb(")", 1, app_key) < 0) return -1;
            }
        }
        return 0;
    } else {
        return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;
    }
}
