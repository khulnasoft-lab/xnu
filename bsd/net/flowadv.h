/*
 * Copyright (c) 2012-2021 Apple Inc. All rights reserved.
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

#ifndef _NET_FLOWADV_H_
#define _NET_FLOWADV_H_

#ifdef KERNEL_PRIVATE
#include <sys/types.h>
#include <sys/queue.h>

#if SKYWALK
#include <skywalk/os_skywalk.h>
#endif /* SKYWALK */

#define FADV_SUCCESS            0       /* success */
#define FADV_FLOW_CONTROLLED    1       /* regular flow control */
#define FADV_SUSPENDED          2       /* flow control due to suspension */

struct flowadv {
	int32_t         code;           /* FADV advisory code */
};

typedef enum fce_event_type {
	FCE_EVENT_TYPE_FLOW_CONTROL_FEEDBACK   = 0,
	FCE_EVENT_TYPE_CONGESTION_EXPERIENCED  = 1,
} fce_event_type_t;

#ifdef BSD_KERNEL_PRIVATE
struct flowadv_fcentry {
	STAILQ_ENTRY(flowadv_fcentry) fce_link;
	u_int32_t        fce_flowsrc_type;       /* FLOWSRC values */
	u_int32_t        fce_flowid;
	u_int32_t        fce_ce_cnt;
	u_int32_t        fce_pkts_since_last_report;
	fce_event_type_t fce_event_type;
#if SKYWALK
	flowadv_token_t fce_flowsrc_token;
	flowadv_idx_t   fce_flowsrc_fidx;
	struct ifnet    *fce_ifp;
#endif /* SKYWALK */
};

STAILQ_HEAD(flowadv_fclist, flowadv_fcentry);

__BEGIN_DECLS

extern void flowadv_init(void);
extern struct flowadv_fcentry *flowadv_alloc_entry(int);
extern void flowadv_free_entry(struct flowadv_fcentry *);
extern void flowadv_add(struct flowadv_fclist *);
extern void flowadv_add_entry(struct flowadv_fcentry *);

__END_DECLS

#endif /* BSD_KERNEL_PRIVATE */
#endif /* KERNEL_PRIVATE */
#endif /* _NET_FLOWADV_H_ */
