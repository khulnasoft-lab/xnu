/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
 */
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _MACH_MACHINE_SYS_SDT_H
#define _MACH_MACHINE_SYS_SDT_H

#ifdef KERNEL

#if CONFIG_DTRACE

/*
 * Include arch specific macro definitions for SDT probes.
 * The argument types should be no greater than uintptr_t in size each.
 * The behavior of larger types is undefined.
 */
#include <mach/machine/sdt_isa.h>

#else

#define DTRACE_PROBE(provider, name) do {} while(0)
#define DTRACE_PROBE1(provider, name, arg0) do {} while(0)
#define DTRACE_PROBE2(provider, name, arg0, arg1) do {} while(0)
#define DTRACE_PROBE3(provider, name, arg0, arg1, arg2) do {} while(0)
#define DTRACE_PROBE4(provider, name, arg0, arg1, arg2, arg3) do {} while(0)
#define DTRACE_PROBE5(provider, name, arg0, arg1, arg2, arg3, arg4) do {} while(0)
#define DTRACE_PROBE6(provider, name, arg0, arg1, arg2, arg3, arg4, arg5) do {} while(0)
#define DTRACE_PROBE7(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6) do {} while(0)
#define DTRACE_PROBE8(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7) do {} while(0)

#endif /* CONFIG_DTRACE */

#define DTRACE_SCHED(name)                                              \
	DTRACE_PROBE(__sched_, name);

#define DTRACE_SCHED1(name, type1, arg1)                                \
	DTRACE_PROBE1(__sched_, name, arg1);

#define DTRACE_SCHED2(name, type1, arg1, type2, arg2)                   \
	DTRACE_PROBE2(__sched_, name, arg1, arg2);

#define DTRACE_SCHED3(name, type1, arg1, type2, arg2, type3, arg3)      \
	DTRACE_PROBE3(__sched_, name, arg1, arg2, arg3);

#define DTRACE_SCHED4(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__sched_, name, arg1, arg2, arg3, arg4);

#define DTRACE_SCHED5(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4, type5, arg5)                              \
	DTRACE_PROBE5(__sched_, name, arg1, arg2, arg3, arg4, arg5, arg6);

#define DTRACE_SCHED6(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4, type5, arg5, type6, arg6)                 \
	DTRACE_PROBE6(__sched_, name, arg1, arg2, arg3, arg4, arg5, arg6);

#define DTRACE_PROC(name)                                               \
	DTRACE_PROBE(__proc_, name);

#define DTRACE_PROC1(name, type1, arg1)                                 \
	DTRACE_PROBE1(__proc_, name, arg1);

#define DTRACE_PROC2(name, type1, arg1, type2, arg2)                    \
	DTRACE_PROBE2(__proc_, name, arg1, arg2);

#define DTRACE_PROC3(name, type1, arg1, type2, arg2, type3, arg3)       \
	DTRACE_PROBE3(__proc_, name, arg1, arg2, arg3);

#define DTRACE_PROC4(name, type1, arg1, type2, arg2,                    \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__proc_, name, arg1, arg2, arg3, arg4);

#define DTRACE_IO(name)                                                 \
	DTRACE_PROBE(__io_, name);

#define DTRACE_IO1(name, type1, arg1)                                   \
	DTRACE_PROBE1(__io_, name, arg1);

#define DTRACE_IO2(name, type1, arg1, type2, arg2)                      \
	DTRACE_PROBE2(__io_, name, arg1, arg2);

#define DTRACE_IO3(name, type1, arg1, type2, arg2, type3, arg3)         \
	DTRACE_PROBE3(__io_, name, arg1, arg2, arg3);

#define DTRACE_IO4(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__io_, name, arg1, arg2, arg3, arg4);

#define DTRACE_INT5(name, type1, arg1, type2, arg2,                     \
	    type3, arg3, type4, arg4, type5, arg5)                                              \
	DTRACE_PROBE5(__sdt_, name, arg1, arg2, arg3, arg4, arg5);

#define DTRACE_MEMORYSTATUS2(name, type1, arg1, type2, arg2)            \
	DTRACE_PROBE2(__sdt_, name, arg1, arg2);

#define DTRACE_MEMORYSTATUS3(name, type1, arg1, type2, arg2, type3, arg3)               \
	DTRACE_PROBE3(__sdt_, name, arg1, arg2, arg3);

#define DTRACE_MEMORYSTATUS4(name, type1, arg1, type2, arg2, type3, arg3, type4, arg4)  \
	DTRACE_PROBE4(__sdt_, name, arg1, arg2, arg3, arg4);

#define DTRACE_MEMORYSTATUS6(name, type1, arg1, type2, arg2,                    \
	    type3, arg3, type4, arg4, type5, arg5, type6, arg6) \
	DTRACE_PROBE6(__vminfo_, name, arg1, arg2, arg3, arg4, arg5, arg6)

#define DTRACE_TMR3(name, type1, arg1, type2, arg2, type3, arg3)                \
	DTRACE_PROBE3(__sdt_, name, arg1, arg2, arg3);

#define DTRACE_TMR4(name, type1, arg1, arg2, arg3, arg4)                        \
	DTRACE_PROBE4(__sdt_, name, arg1, arg2, arg3, arg4);

#define DTRACE_TMR5(name, type1, arg1, type2, arg2, type3, arg3, arg4, arg5)    \
	DTRACE_PROBE5(__sdt_, name, arg1, arg2, arg3, arg4, arg5);

#define DTRACE_TMR6(name, type1, arg1, type2, arg2, type3, arg3, arg4, arg5, arg6) \
	DTRACE_PROBE6(__sdt_, name, arg1, arg2, arg3, arg4, arg5, arg6);

#define DTRACE_TMR7(name, type1, arg1, type2, arg2, type3, arg3, arg4, arg5, arg6, arg7) \
	DTRACE_PROBE7(__sdt_, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7);

#define DTRACE_PHYSLAT3(name, type1, arg1, type2, arg2, type3, arg3)            \
	DTRACE_PROBE3(__sdt_, name, arg1, arg2, arg3);

#define DTRACE_PHYSLAT4(name, type1, arg1, type2, arg2, type3, arg3, type4, arg4)        \
	DTRACE_PROBE4(__sdt_, name, arg1, arg2, arg3, arg4);

#define DTRACE_PHYSLAT5(name, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  \
	DTRACE_PROBE5(__sdt_, name, arg1, arg2, arg3, arg4, arg5);

#define DTRACE_MACF(name)                         \
	DTRACE_PROBE(__sdt_, name);

#define DTRACE_MACF1(name, type1, arg1)                         \
	DTRACE_PROBE1(__sdt_, name, arg1);

#define DTRACE_MACF2(name, type1, arg1, type2, arg2)            \
	DTRACE_PROBE2(__sdt_, name, arg1, arg2);

#define DTRACE_MACF3(name, type1, arg1, type2, arg2, type3, arg3)            \
	DTRACE_PROBE3(__sdt_, name, arg1, arg2, arg3);

#define DTRACE_VM(name)                                                 \
	DTRACE_PROBE(__vminfo_, name)

#define DTRACE_VM1(name, type1, arg1)                                   \
	DTRACE_PROBE1(__vminfo_, name, arg1)

#define DTRACE_VM2(name, type1, arg1, type2, arg2)                      \
	DTRACE_PROBE2(__vminfo_, name, arg1, arg2)

#define DTRACE_VM3(name, type1, arg1, type2, arg2, type3, arg3)         \
	DTRACE_PROBE3(__vminfo_, name, arg1, arg2, arg3)

#define DTRACE_VM4(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__vminfo_, name, arg1, arg2, arg3, arg4)

#define DTRACE_VM5(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4, type5, arg5)                              \
	DTRACE_PROBE5(__vminfo_, name, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_VM6(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4, type5, arg5, type6, arg6) \
	DTRACE_PROBE6(__vminfo_, name, arg1, arg2, arg3, arg4, arg5, arg6)

#define DTRACE_VM7(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7) \
	DTRACE_PROBE7(__vminfo_, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define DTRACE_VM8(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8) \
	DTRACE_PROBE8(__vminfo_, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

#define DTRACE_IP(name)                                                 \
	DTRACE_PROBE(__ip_, name)

#define DTRACE_IP1(name, type1, arg1)                                   \
	DTRACE_PROBE1(__ip_, name, arg1)

#define DTRACE_IP2(name, type1, arg1, type2, arg2)                      \
	DTRACE_PROBE2(__ip_, name, arg1, arg2)

#define DTRACE_IP3(name, type1, arg1, type2, arg2, type3, arg3)         \
	DTRACE_PROBE3(__ip_, name, arg1, arg2, arg3)

#define DTRACE_IP4(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__ip_, name, arg1, arg2, arg3, arg4)

#define DTRACE_IP5(name, typ1, arg1, type2, arg2, type3, arg3,          \
	    type4, arg4, type5, arg5)                                           \
	DTRACE_PROBE5(__ip_, name, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_IP6(name, type1, arg1, type2, arg2, type3, arg3,         \
	    type4, arg4, type5, arg5, type6, arg6)                               \
	DTRACE_PROBE6(__ip_, name, arg1, arg2, arg3, arg4, arg5, arg6)

#define DTRACE_IP7(name, type1, arg1, type2, arg2, type3, arg3,         \
	    type4, arg4, type5, arg5, type6, arg6, type7, arg7)                 \
	DTRACE_PROBE7(__ip_, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define DTRACE_ROUTE(name)                                              \
	DTRACE_PROBE(__route_, name)

#define DTRACE_ROUTE1(name, type1, arg1)                                \
	DTRACE_PROBE1(__route_, name, arg1)

#define DTRACE_ROUTE2(name, type1, arg1, type2, arg2)                   \
	DTRACE_PROBE2(__route_, name, arg1, arg2)

#define DTRACE_ROUTE3(name, type1, arg1, type2, arg2, type3, arg3)      \
	DTRACE_PROBE3(__route_, name, arg1, arg2, arg3)

#define DTRACE_ROUTE4(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__route_, name, arg1, arg2, arg3, arg4)

#define DTRACE_ROUTE5(name, typ1, arg1, type2, arg2, type3, arg3,       \
	    type4, arg4, type5, arg5)                                           \
	DTRACE_PROBE5(__route_, name, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_ROUTE6(name, type1, arg1, type2, arg2, type3, arg3,      \
	    type4, arg4, type5, arg5, type6, arg6)                               \
	DTRACE_PROBE6(__route_, name, arg1, arg2, arg3, arg4, arg5, arg6)

#define DTRACE_ROUTE7(name, type1, arg1, type2, arg2, type3, arg3,      \
	    type4, arg4, type5, arg5, type6, arg6, type7, arg7)                 \
	DTRACE_PROBE7(__route_, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define DTRACE_TCP(name)                                                 \
	DTRACE_PROBE(__tcp_, name)

#define DTRACE_TCP1(name, type1, arg1)                                   \
	DTRACE_PROBE1(__tcp_, name, arg1)

#define DTRACE_TCP2(name, type1, arg1, type2, arg2)                      \
	DTRACE_PROBE2(__tcp_, name, arg1, arg2)

#define DTRACE_TCP3(name, type1, arg1, type2, arg2, type3, arg3)         \
	DTRACE_PROBE3(__tcp_, name, arg1, arg2, arg3)

#define DTRACE_TCP4(name, type1, arg1, type2, arg2,                      \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__tcp_, name, arg1, arg2, arg3, arg4)

#define DTRACE_TCP5(name, typ1, arg1, type2, arg2, type3, arg3,          \
	    type4, arg4, type5, arg5)                                           \
	DTRACE_PROBE5(__tcp_, name, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_MPTCP(name)                                              \
	DTRACE_PROBE(__mptcp_, name)

#define DTRACE_MPTCP1(name, type1, arg1)                                \
	DTRACE_PROBE1(__mptcp_, name, arg1)

#define DTRACE_MPTCP2(name, type1, arg1, type2, arg2)                   \
	DTRACE_PROBE2(__mptcp_, name, arg1, arg2)

#define DTRACE_MPTCP3(name, type1, arg1, type2, arg2, type3, arg3)      \
	DTRACE_PROBE3(__mptcp_, name, arg1, arg2, arg3)

#define DTRACE_MPTCP4(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4)                          \
	DTRACE_PROBE4(__mptcp_, name, arg1, arg2, arg3, arg4)

#define DTRACE_MPTCP5(name, typ1, arg1, type2, arg2, type3, arg3,       \
	    type4, arg4, type5, arg5)                         \
	DTRACE_PROBE5(__mptcp_, name, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_MPTCP6(name, typ1, arg1, type2, arg2, type3, arg3,       \
	    type4, arg4, type5, arg5, type6, arg6)            \
	DTRACE_PROBE6(__mptcp_, name, arg1, arg2, arg3, arg4, arg5, arg6)

#define DTRACE_MPTCP7(name, typ1, arg1, type2, arg2, type3, arg3,       \
	    type4, arg4, type5, arg5, type6, arg6,            \
	    type7, arg7)                                      \
	DTRACE_PROBE7(__mptcp_, name, arg1, arg2, arg3, arg4, arg5,     \
	              arg6, arg7)

#define DTRACE_FSINFO(name, type, vp)                                   \
	DTRACE_PROBE1(__fsinfo_, name, vp)

#define DTRACE_FSINFO_IO(name, type1, vp, type2, size)                  \
	DTRACE_PROBE2(__fsinfo_, name, vp, size)

#define DTRACE_BOOST(name)                                              \
	DTRACE_PROBE(__boost_, name);

#define DTRACE_BOOST1(name, type1, arg1)                                \
	DTRACE_PROBE1(__boost_, name, arg1);

#define DTRACE_BOOST2(name, type1, arg1, type2, arg2)                   \
	DTRACE_PROBE2(__boost_, name, arg1, arg2);

#define DTRACE_BOOST3(name, type1, arg1, type2, arg2, type3, arg3)      \
	DTRACE_PROBE3(__boost_, name, arg1, arg2, arg3);

#define DTRACE_BOOST4(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4)                                           \
	DTRACE_PROBE4(__boost_, name, arg1, arg2, arg3, arg4);

#define DTRACE_BOOST5(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4, type5, arg5)                                              \
	DTRACE_PROBE5(__boost_, name, arg1, arg2, arg3, arg4, arg5);

#define DTRACE_BOOST6(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4, type5, arg5, type6, arg6)                 \
	DTRACE_PROBE6(__boost_, name, arg1, arg2, arg3, arg4, arg5, arg6);

#if KASAN
#define DTRACE_KASAN(name)                                              \
	DTRACE_PROBE(__kasan_, name);

#define DTRACE_KASAN1(name, type1, arg1)                                \
	DTRACE_PROBE1(__kasan_, name, arg1);

#define DTRACE_KASAN2(name, type1, arg1, type2, arg2)                   \
	DTRACE_PROBE2(__kasan_, name, arg1, arg2);

#define DTRACE_KASAN3(name, type1, arg1, type2, arg2, type3, arg3)      \
	DTRACE_PROBE3(__kasan_, name, arg1, arg2, arg3);

#define DTRACE_KASAN4(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4)                                       \
	DTRACE_PROBE4(__kasan_, name, arg1, arg2, arg3, arg4);

#define DTRACE_KASAN5(name, type1, arg1, type2, arg2,                   \
	    type3, arg3, type4, arg4, type5, arg5)                          \
	DTRACE_PROBE5(__kasan_, name, arg1, arg2, arg3, arg4, arg5);
#endif /* KASAN */

#if PRIVATE
#define DTRACE_SKYWALK(name)                                            \
	DTRACE_PROBE(__skywalk_, name)

#define DTRACE_SKYWALK1(name, type1, arg1)                              \
	DTRACE_PROBE1(__skywalk_, name, arg1)

#define DTRACE_SKYWALK2(name, type1, arg1, type2, arg2)                 \
	DTRACE_PROBE2(__skywalk_, name, arg1, arg2)

#define DTRACE_SKYWALK3(name, type1, arg1, type2, arg2, type3, arg3)    \
	DTRACE_PROBE3(__skywalk_, name, arg1, arg2, arg3)

#define DTRACE_SKYWALK4(name, type1, arg1, type2, arg2, type3, arg3,    \
	    type4, arg4)                                                \
	DTRACE_PROBE4(__skywalk_, name, arg1, arg2, arg3, arg4)

#define DTRACE_SKYWALK5(name, type1, arg1, type2, arg2, type3, arg3,    \
	    type4, arg4, type5, arg5)                                   \
	DTRACE_PROBE5(__skywalk_, name, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_SKYWALK6(name, type1, arg1, type2, arg2, type3, arg3,    \
	    type4, arg4, type5, arg5, type6, arg6)                      \
	DTRACE_PROBE6(__skywalk_, name, arg1, arg2, arg3, arg4, arg5, arg6)

#define DTRACE_SKYWALK7(name, type1, arg1, type2, arg2, type3, arg3,    \
	    type4, arg4, type5, arg5, type6, arg6, type7, arg7)         \
	DTRACE_PROBE7(__skywalk_, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define DTRACE_SKYWALK8(name, type1, arg1, type2, arg2, type3, arg3,         \
	    type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8) \
	DTRACE_PROBE8(__skywalk_, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

#endif /* PRIVATE */

#ifdef PRIVATE
#define DTRACE_KCOV1(name, type1, arg1)                            \
	DTRACE_PROBE1(__kcov_, name, arg1)

#endif /* PRIVATE */

#ifdef PRIVATE

/*
 * The AppleHV kext uses its own variants of these macros to avoid storing
 * arguments as intermediate values on the stack (to lessen the overhead).
 * If/When the mechanism by which SDT probes are generated changes, AppleHV
 * should be updated too.
 */
#define DTRACE_HV(name)                                            \
	DTRACE_PROBE(__hv_, name)

#define DTRACE_HV1(name, type1, arg1)                              \
	DTRACE_PROBE1(__hv_, name, arg1)

#define DTRACE_HV2(name, type1, arg1, type2, arg2)                 \
	DTRACE_PROBE2(__hv_, name, arg1, arg2)

#define DTRACE_HV3(name, type1, arg1, type2, arg2, type3, arg3)    \
	DTRACE_PROBE3(__hv_, name, arg1, arg2, arg3)

#define DTRACE_HV4(name, type1, arg1, type2, arg2, type3, arg3,    \
	    type4, arg4)                                                \
	DTRACE_PROBE4(__hv_, name, arg1, arg2, arg3, arg4)

#define DTRACE_HV5(name, type1, arg1, type2, arg2, type3, arg3,    \
	    type4, arg4, type5, arg5)                                   \
	DTRACE_PROBE5(__hv_, name, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_HV6(name, type1, arg1, type2, arg2, type3, arg3,    \
	    type4, arg4, type5, arg5, type6, arg6)                      \
	DTRACE_PROBE6(__hv_, name, arg1, arg2, arg3, arg4, arg5, arg6)
#endif /* PRIVATE */

#endif /* KERNEL */

#endif  /* _MACH_MACHINE_SYS_SDT_H */
