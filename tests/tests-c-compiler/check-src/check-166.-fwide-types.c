/*
 * Verify the constraints of the INTEGER values which are wider than 32 bits.
 * The values never go through a long, which is 32 bits on some platforms.
 */
#undef	NDEBUG
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>

#include <Uint64.h>
#include <Time64.h>
#include <Signed40.h>
#include <Small.h>

static void
check_signed(asn_TYPE_descriptor_t *td, intmax_t value, int expect_ok) {
	INTEGER_t st;
	int ret;

	memset(&st, 0, sizeof(st));
	ret = asn_imax2INTEGER(&st, value);
	assert(ret == 0);

	ret = asn_check_constraints(td, &st, 0, 0);
	printf("%s %jd: %s\n", td->name, value, ret ? "refused" : "ok");
	assert((ret == 0) == expect_ok);

	ASN_STRUCT_FREE_CONTENTS_ONLY(*td, &st);
}

static void
check_unsigned(asn_TYPE_descriptor_t *td, uintmax_t value, int expect_ok) {
	INTEGER_t st;
	int ret;

	memset(&st, 0, sizeof(st));
	ret = asn_umax2INTEGER(&st, value);
	assert(ret == 0);

	ret = asn_check_constraints(td, &st, 0, 0);
	printf("%s %ju: %s\n", td->name, value, ret ? "refused" : "ok");
	assert((ret == 0) == expect_ok);

	ASN_STRUCT_FREE_CONTENTS_ONLY(*td, &st);
}

static void
check_xer(const char *xml, uintmax_t expect) {
	Time64_t *st = 0;
	asn_dec_rval_t rv;
	uintmax_t value;
	int ret;

	rv = xer_decode(0, &asn_DEF_Time64, (void **)&st, xml, strlen(xml));
	assert(rv.code == RC_OK);
	ret = asn_INTEGER2umax(st, &value);
	assert(ret == 0);
	printf("%s -> %ju\n", xml, value);
	assert(value == expect);
	ret = asn_check_constraints(&asn_DEF_Time64, st, 0, 0);
	assert(ret == 0);

	ASN_STRUCT_FREE(asn_DEF_Time64, st);
}

int
main(int ac, char **av) {
	asn_TYPE_descriptor_t *wide[] = { &asn_DEF_Uint64, &asn_DEF_Time64 };
	size_t i;

	(void)ac;	/* Unused argument */
	(void)av;	/* Unused argument */

	for(i = 0; i < sizeof(wide) / sizeof(wide[0]); i++) {
		check_unsigned(wide[i], 0, 1);
		check_unsigned(wide[i], 2147483648UL, 1);
		check_unsigned(wide[i], 4294967296ULL, 1);
		check_unsigned(wide[i], 1099511627776ULL, 1);
		check_unsigned(wide[i], 9223372036854775807ULL, 1);
		check_unsigned(wide[i], 9223372036854775808ULL, 1);
		check_unsigned(wide[i], 18446744073709551615ULL, 1);
		check_signed(wide[i], -1, 0);
		check_signed(wide[i], -1099511627776LL, 0);
	}

	check_signed(&asn_DEF_Signed40, 0, 1);
	check_signed(&asn_DEF_Signed40, 549755813887LL, 1);
	check_signed(&asn_DEF_Signed40, 549755813888LL, 0);
	check_signed(&asn_DEF_Signed40, -549755813888LL, 1);
	check_signed(&asn_DEF_Signed40, -549755813889LL, 0);
	check_signed(&asn_DEF_Signed40, 4294967296LL, 1);
	check_signed(&asn_DEF_Signed40, -4294967296LL, 1);

	/* A small range is kept in a long even with -fwide-types. */
	for(i = 0; i < 4; i++) {
		static const long values[] = { 0, 255, 256, -1 };
		Small_t small = values[i];
		int ret = asn_check_constraints(&asn_DEF_Small, &small, 0, 0);
		assert((ret == 0) == (i < 2));
	}

	check_xer("<Time64>1099511627776</Time64>", 1099511627776ULL);
	check_xer("<Time64>9223372036854775807</Time64>",
		9223372036854775807ULL);

	return 0;
}
