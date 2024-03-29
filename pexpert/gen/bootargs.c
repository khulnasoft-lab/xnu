/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>

#if defined(CONFIG_XNUPOST)
#include <tests/xnupost.h>
#endif

typedef boolean_t (*argsep_func_t) (char c);

static boolean_t isargsep( char c);
static boolean_t israngesep( char c);
#if defined(__x86_64__)
static int argstrcpy(char *from, char *to);
#endif
static int argstrcpy2(char *from, char *to, unsigned maxlen);
static int argnumcpy(long long val, void *to, unsigned maxlen);
static int getval(char *s, long long *val, argsep_func_t issep, boolean_t skip_equal_sign);
boolean_t get_range_bounds(char * c, int64_t * lower, int64_t * upper);

extern int IODTGetDefault(const char *key, void *infoAddr, unsigned int infoSize);
#if defined(CONFIG_XNUPOST)
kern_return_t parse_boot_arg_test(void);
#endif

struct i24 {
	int32_t i24 : 24;
	int32_t _pad : 8;
};

#define NUM     0
#define STR     1

static boolean_t
PE_parse_boot_argn_internal(
	char *args,
	const char *arg_string,
	void *      arg_ptr,
	int         max_len,
	boolean_t   force_string)
{
	char *cp, c;
	uintptr_t i;
	long long val = 0;
	boolean_t arg_boolean;
	boolean_t arg_found;

	if (*args == '\0') {
		return FALSE;
	}

#if !defined(__x86_64__)
	if (max_len == -1) {
		return FALSE;
	}
#endif

	arg_found = FALSE;

	while (*args && isargsep(*args)) {
		args++;
	}

	while (*args) {
		if (*args == '-') {
			arg_boolean = TRUE;
		} else {
			arg_boolean = FALSE;
		}

		cp = args;
		while (!isargsep(*cp) && *cp != '=') {
			cp++;
		}

		c = *cp;

		i = cp - args;
		if (strncmp(args, arg_string, i) ||
		    (i != strlen(arg_string))) {
			goto gotit;
		}

		if (arg_boolean) {
			if (max_len > 0) {
				if (force_string) {
					argstrcpy2("1", arg_ptr, max_len);
				} else {
					argnumcpy(1, arg_ptr, max_len);/* max_len of 0 performs no copy at all*/
				}
				arg_found = TRUE;
			} else if (max_len == 0) {
				arg_found = TRUE;
			}
			break;
		} else {
			while (*cp && isargsep(*cp)) {
				cp++;
			}
			if (*cp == '=' && c != '=') {
				args = cp + 1;
				goto gotit;
			}
			if ('_' == *arg_string) { /* Force a string copy if the argument name begins with an underscore */
				if (max_len > 0) {
					int hacklen = 17 > max_len ? 17 : max_len;
					argstrcpy2(++cp, (char *)arg_ptr, hacklen - 1);  /* Hack - terminate after 16 characters */
					arg_found = TRUE;
				} else if (max_len == 0) {
					arg_found = TRUE;
				}
				break;
			}
			switch (force_string ? STR : getval(cp, &val, isargsep, FALSE)) {
			case NUM:
				if (max_len > 0) {
					argnumcpy(val, arg_ptr, max_len);
					arg_found = TRUE;
				} else if (max_len == 0) {
					arg_found = TRUE;
				}
				break;
			case STR:
				if (*cp == '=') {
					if (max_len > 0) {
						argstrcpy2(++cp, (char *)arg_ptr, max_len - 1);        /*max_len of 0 performs no copy at all*/
						arg_found = TRUE;
					} else if (max_len == 0) {
						arg_found = TRUE;
					}
#if defined(__x86_64__)
					else if (max_len == -1) {         /* unreachable on embedded */
						argstrcpy(++cp, (char *)arg_ptr);
						arg_found = TRUE;
					}
#endif
				} else {
					if (max_len > 0) {
						argstrcpy2("1", arg_ptr, max_len);
						arg_found = TRUE;
					} else if (max_len == 0) {
						arg_found = TRUE;
					}
				}
				break;
			}
			goto gotit;
		}
gotit:
		/* Skip over current arg */
		while (!isargsep(*args)) {
			args++;
		}

		/* Skip leading white space (catch end of args) */
		while (*args && isargsep(*args)) {
			args++;
		}
	}

	return arg_found;
}

boolean_t
PE_parse_boot_argn(
	const char      *arg_string,
	void            *arg_ptr,
	int                     max_len)
{
	return PE_parse_boot_argn_internal(PE_boot_args(), arg_string, arg_ptr, max_len, FALSE);
}

boolean_t
PE_boot_arg_uint64_eq(const char *arg_string, uint64_t value)
{
	uint64_t tmp;
	if (!PE_parse_boot_argn(arg_string, &tmp, sizeof(tmp))) {
		return false;
	}

	return tmp == value;
}

boolean_t
PE_parse_boot_arg_str(
	const char      *arg_string,
	char            *arg_ptr,
	int                     strlen)
{
	return PE_parse_boot_argn_internal(PE_boot_args(), arg_string, arg_ptr, strlen, TRUE);
}

#if defined(CONFIG_XNUPOST)
kern_return_t
parse_boot_arg_test(void)
{
	// Tests are derived from libc/tests/darwin_bsd.c
	static struct string_test_case {
		char *args;
		const char *argname;
		char *argvalue;
		boolean_t found;
	} string_test_cases[] = {
		{"-x -a b=3 y=42", "-a", "1", TRUE},
		{"-x -a b=3 y=42", "b", "3", TRUE},
		{"-x -a b=2 ba=3 y=42", "b", "2", TRUE},
		{"-x -a ba=3 b=2 y=42", "b", "2", TRUE},
		{"-x -a b=2 ba=3 y=42", "ba", "3", TRUE},
		{"-x -a ba=3 b=2 y=42", "ba", "3", TRUE},
		{"-x -ab -aa y=42", "-a", NULL, FALSE},
		{"-x b=96 y=42", "bx", NULL, FALSE},
		{"-x ab=96 y=42", "a", NULL, FALSE},
		{"hello=world -foobar abc debug=0xBAADF00D", "notarealthing", NULL,
		 FALSE},
		{"hello=world -foobar abc debug=0xBAADF00D", "hello", "world", TRUE},
		{"hello=world -foobar abc debug=0xBAADF00D", "debug", "0xBAADF00D",
		 TRUE},
		{"hello=world -foobar abc debug=0xBAADF00D", "-foobar", "1", TRUE},
		{"hello=world -foobar abc debug=0xBAADF00D", "abc", "1", TRUE},
	};

	T_LOG("Testing boot-arg string parsing.\n");
	for (int i = 0; i < (int)(sizeof(string_test_cases) /
	    sizeof(string_test_cases[0])); i++) {
		struct string_test_case *test_case = &string_test_cases[i];

		char result[256] = "NOT_FOUND";
		boolean_t found = PE_parse_boot_argn_internal(test_case->args,
		    test_case->argname, result, sizeof(result), TRUE);

		if (test_case->found) {
			T_LOG("\"%s\": Looking for \"%s\", expecting \"%s\" found",
			    test_case->args, test_case->argname, test_case->argvalue);
			T_EXPECT(found, "Should find argument");
			T_EXPECT_EQ_STR(result, test_case->argvalue,
			    "Should find correct result");
		} else {
			T_LOG("\"%s\": Looking for \"%s\", expecting not found",
			    test_case->args, test_case->argname, test_case->argvalue);
			T_EXPECT(!found, "Should not find argument");
		}
	}

	static struct integer_test_case {
		char *args;
		const char *argname;
		int argvalue;
		boolean_t found;
	} integer_test_cases[] = {
		{"-x -a b=3 y=42", "-a", 1, TRUE},
		{"-x -a b=3 y=42", "b", 3, TRUE},
		{"-x -a b=2 ba=3 y=42", "b", 2, TRUE},
		{"-x -a ba=3 b=2 y=42", "b", 2, TRUE},
		{"-x -a b=2 ba=3 y=42", "ba", 3, TRUE},
		{"-x -a ba=3 b=2 y=42", "ba", 3, TRUE},
		{"-x -ab -aa y=42", "-a", 0, FALSE},
		{"-x b=96 y=42", "bx", 0, FALSE},
		{"-x ab=96 y=42", "a", 0, FALSE},
		{"hello=world -foobar abc debug=0xBAADF00D", "notarealthing", 0, FALSE},
		{"hello=world -foobar abc debug=0xBAADF00D", "hello",
		 0x00726F77 /* "wor" */, TRUE},
		{"hello=world -foobar abc debug=0xBAADF00D", "debug", 0xBAADF00D, TRUE},
		{"hello=world -foobar abc debug=0xBAADF00D", "-foobar", 1, TRUE},
		{"hello=world -foobar abc debug=0xBAADF00D", "abc", 1, TRUE},
	};

	T_LOG("Testing boot-arg integer parsing.\n");
	for (int i = 0; i < (int)(sizeof(integer_test_cases) /
	    sizeof(integer_test_cases[0])); i++) {
		struct integer_test_case *test_case = &integer_test_cases[i];

		int result = 0xCAFEFACE;
		boolean_t found = PE_parse_boot_argn_internal(test_case->args,
		    test_case->argname, &result, sizeof(result), FALSE);

		if (test_case->found) {
			T_LOG("\"%s\": Looking for \"%s\", expecting %d found",
			    test_case->args, test_case->argname, test_case->argvalue);
			T_EXPECT(found, "Should find argument");
			T_EXPECT_EQ_INT(result, test_case->argvalue,
			    "Should find correct result");
		} else {
			T_LOG("\"%s\": Looking for \"%s\", expecting not found",
			    test_case->args, test_case->argname, test_case->argvalue);
			T_EXPECT(!found, "Should not find argument");
		}
	}

	return KERN_SUCCESS;
}
#endif /* defined(CONFIG_XNUPOST) */

static boolean_t
isargsep(char c)
{
	if (c == ' ' || c == '\0' || c == '\t') {
		return TRUE;
	} else {
		return FALSE;
	}
}

static boolean_t
israngesep(char c)
{
	if (isargsep(c) || c == '_' || c == ',') {
		return TRUE;
	} else {
		return FALSE;
	}
}

#if defined(__x86_64__)
static int
argstrcpy(
	char *from,
	char *to)
{
	int i = 0;

	while (!isargsep(*from)) {
		i++;
		*to++ = *from++;
	}
	*to = 0;
	return i;
}
#endif

static int
argstrcpy2(
	char *from,
	char *to,
	unsigned maxlen)
{
	unsigned int i = 0;

	while (!isargsep(*from) && i < maxlen) {
		i++;
		*to++ = *from++;
	}
	*to = 0;
	return i;
}

static int
argnumcpy(long long val, void *to, unsigned maxlen)
{
	switch (maxlen) {
	case 0:
		/* No write-back, caller just wants to know if arg was found */
		break;
	case 1:
		*(int8_t *)to = (int8_t)val;
		break;
	case 2:
		*(int16_t *)to = (int16_t)val;
		break;
	case 3:
		/* Unlikely in practice */
		((struct i24 *)to)->i24 = (int32_t)val;
		break;
	case 4:
		*(int32_t *)to = (int32_t)val;
		break;
	case 8:
		*(int64_t *)to = (int64_t)val;
		break;
	default:
		*(int32_t *)to = (int32_t)val;
		maxlen = 4;
		break;
	}

	return (int)maxlen;
}

static int
getval(
	char *s,
	long long *val,
	argsep_func_t issep,
	boolean_t skip_equal_sign )
{
	unsigned long long radix, intval;
	unsigned char c;
	int sign = 1;
	boolean_t has_value = FALSE;

	if (*s == '=') {
		s++;
		has_value = TRUE;
	}

	if (has_value || skip_equal_sign) {
		if (*s == '-') {
			sign = -1;
			s++;
		}
		intval = *s++ - '0';
		radix = 10;
		if (intval == 0) {
			switch (*s) {
			case 'x':
				radix = 16;
				s++;
				break;

			case 'b':
				radix = 2;
				s++;
				break;

			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				intval = *s - '0';
				s++;
				radix = 8;
				break;

			default:
				if (!issep(*s)) {
					return STR;
				}
			}
		} else if (intval >= radix) {
			return STR;
		}
		for (;;) {
			c = *s++;
			if (issep(c)) {
				break;
			}
			if ((radix <= 10) &&
			    ((c >= '0') && (c <= ('9' - (10 - radix))))) {
				c -= '0';
			} else if ((radix == 16) &&
			    ((c >= '0') && (c <= '9'))) {
				c -= '0';
			} else if ((radix == 16) &&
			    ((c >= 'a') && (c <= 'f'))) {
				c -= 'a' - 10;
			} else if ((radix == 16) &&
			    ((c >= 'A') && (c <= 'F'))) {
				c -= 'A' - 10;
			} else if (c == 'k' || c == 'K') {
				sign *= 1024;
				break;
			} else if (c == 'm' || c == 'M') {
				sign *= 1024 * 1024;
				break;
			} else if (c == 'g' || c == 'G') {
				sign *= 1024 * 1024 * 1024;
				break;
			} else {
				return STR;
			}
			if (c >= radix) {
				return STR;
			}
			intval *= radix;
			intval += c;
		}
		if (!issep(c) && !issep(*s)) {
			return STR;
		}
		*val = intval * sign;
		return NUM;
	}
	*val = 1;
	return NUM;
}

boolean_t
PE_imgsrc_mount_supported()
{
	return TRUE;
}

boolean_t
PE_get_default(
	const char      *property_name,
	void            *property_ptr,
	unsigned int max_property)
{
	DTEntry         dte;
	void const      *property_data;
	unsigned int property_size;

	/*
	 * Look for the property using the PE DT support.
	 */
	if (kSuccess == SecureDTLookupEntry(NULL, "/defaults", &dte)) {
		/*
		 * We have a /defaults node, look for the named property.
		 */
		if (kSuccess != SecureDTGetProperty(dte, property_name, &property_data, &property_size)) {
			return FALSE;
		}

		/*
		 * This would be a fine place to do smart argument size management for 32/64
		 * translation, but for now we'll insist that callers know how big their
		 * default values are.
		 */
		if (property_size > max_property) {
			return FALSE;
		}

		/*
		 * Copy back the precisely-sized result.
		 */
		memcpy(property_ptr, property_data, property_size);
		return TRUE;
	}

	/*
	 * Look for the property using I/O Kit's DT support.
	 */
	return IODTGetDefault(property_name, property_ptr, max_property) ? FALSE : TRUE;
}

/* function: get_range_bounds
 * Parse a range string like "1_3,5_20" and return 1,3 as lower and upper.
 * Note: '_' is separator for bounds integer delimiter and
 *       ',' is considered as separator for range pair.
 * returns TRUE when both range values are found
 */
boolean_t
get_range_bounds(char *c, int64_t *lower, int64_t *upper)
{
	if (c == NULL || lower == NULL || upper == NULL) {
		return FALSE;
	}

	if (NUM != getval(c, lower, israngesep, TRUE)) {
		return FALSE;
	}

	while (*c != '\0') {
		if (*c == '_') {
			break;
		}
		c++;
	}

	if (*c == '_') {
		c++;
		if (NUM != getval(c, upper, israngesep, TRUE)) {
			return FALSE;
		}
	} else {
		return FALSE;
	}
	return TRUE;
}
