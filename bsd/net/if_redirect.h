/*
 * Copyright (c) 2022 Apple Computer, Inc. All rights reserved.
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

#ifndef _NET_IF_REDIRECT_VAR_H_
#define _NET_IF_REDIRECT_VAR_H_     1

#ifdef KERNEL_PRIVATE
__private_extern__ void if_redirect_init(void);
#endif /* KERNEL_PRIVATE */

/* Arbitrary identifier for params type */
#define RD_CREATE_PARAMS_TYPE 0x2D27

struct if_redirect_create_params {
	uint16_t ircp_type;
	uint16_t ircp_len;
	uint32_t ircp_ftype;
};

/*
 * SIOCSDRVSPEC
 */
enum {
	RD_S_CMD_NONE              = 0,
	RD_S_CMD_SET_DELEGATE      = 1,
};

struct if_redirect_request {
	uint64_t ifrr_reserved[4];
	union {
		char ifrru_buf[128];                /* stable size */
		char ifrru_delegate_name[IFNAMSIZ]; /* if name */
	} ifrr_u;
#define ifrr_delegate_name ifrr_u.ifrru_delegate_name
};

#endif /* _NET_IF_REDIRECT_VAR_H_ */
