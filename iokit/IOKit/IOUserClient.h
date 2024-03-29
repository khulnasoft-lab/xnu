/*
 * Copyright (c) 1998-2019 Apple Inc. All rights reserved.
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

/*
 * Changes to this API are expected.
 */

#ifndef _IOKIT_IOUSERCLIENT_H
#define _IOKIT_IOUSERCLIENT_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOService.h>
#include <IOKit/OSMessageNotification.h>
#include <DriverKit/IOUserClient.h>
#include <libkern/c++/OSPtr.h>

#if IOKITSTATS
#include <IOKit/IOStatisticsPrivate.h>
#endif

#define _IOUSERCLIENT_SENDASYNCRESULT64WITHOPTIONS_             1

enum {
	kIOUCTypeMask       = 0x0000000f,
	kIOUCScalarIScalarO = 0,
	kIOUCScalarIStructO = 2,
	kIOUCStructIStructO = 3,
	kIOUCScalarIStructI = 4,

	kIOUCForegroundOnly = 0x00000010,
};

/*! @enum
 *   @abstract Constant to denote a variable length structure argument to IOUserClient.
 *   @constant kIOUCVariableStructureSize Use in the structures IOExternalMethod, IOExternalAsyncMethod, IOExternalMethodDispatch to specify the size of the structure is variable.
 */
enum {
	kIOUCVariableStructureSize = 0xffffffff
};


typedef IOReturn (IOService::*IOMethod)(void * p1, void * p2, void * p3,
    void * p4, void * p5, void * p6 );

typedef IOReturn (IOService::*IOAsyncMethod)(OSAsyncReference asyncRef,
    void * p1, void * p2, void * p3,
    void * p4, void * p5, void * p6 );

typedef IOReturn (IOService::*IOTrap)(void * p1, void * p2, void * p3,
    void * p4, void * p5, void * p6 );

struct IOExternalMethod {
	IOService *         object;
	IOMethod            func;
	IOOptionBits        flags;
	IOByteCount         count0;
	IOByteCount         count1;
};

struct IOExternalAsyncMethod {
	IOService *         object;
	IOAsyncMethod       func;
	IOOptionBits        flags;
	IOByteCount         count0;
	IOByteCount         count1;
};

struct IOExternalTrap {
	IOService *         object;
	IOTrap              func;
};

enum {
	kIOUserNotifyMaxMessageSize = 64
};

enum {
	kIOUserNotifyOptionCanDrop = 0x1 /* Fail if queue is full, rather than infinitely queuing. */
};

// keys for clientHasPrivilege
#define kIOClientPrivilegeAdministrator "root"
#define kIOClientPrivilegeLocalUser     "local"
#define kIOClientPrivilegeForeground    "foreground"

/*! @enum
 *   @abstract Constants to specify the maximum number of scalar arguments in the IOExternalMethodArguments structure. These constants are documentary since the scalarInputCount, scalarOutputCount fields reflect the actual number passed.
 *   @constant kIOExternalMethodScalarInputCountMax The maximum number of scalars able to passed on input.
 *   @constant kIOExternalMethodScalarOutputCountMax The maximum number of scalars able to passed on output.
 */
enum {
	kIOExternalMethodScalarInputCountMax  = 16,
	kIOExternalMethodScalarOutputCountMax = 16,
};


struct IOExternalMethodArguments {
	uint32_t            version;

	uint32_t            selector;

	mach_port_t           asyncWakePort;
	io_user_reference_t * asyncReference;
	uint32_t              asyncReferenceCount;

	const uint64_t *    scalarInput;
	uint32_t            scalarInputCount;

	const void *        structureInput;
	uint32_t            structureInputSize;

	IOMemoryDescriptor * structureInputDescriptor;

	uint64_t *          scalarOutput;
	uint32_t            scalarOutputCount;

	void *              structureOutput;
	uint32_t            structureOutputSize;

	IOMemoryDescriptor * structureOutputDescriptor;
	uint32_t             structureOutputDescriptorSize;

	uint32_t            __reservedA;

	OSObject **         structureVariableOutputData;

	uint32_t            __reserved[30];
};

struct IOExternalMethodArgumentsOpaque;

typedef IOReturn (*IOExternalMethodAction)(OSObject * target, void * reference,
    IOExternalMethodArguments * arguments);

struct IOExternalMethodDispatch {
	IOExternalMethodAction function;
	uint32_t               checkScalarInputCount;
	uint32_t               checkStructureInputSize;
	uint32_t               checkScalarOutputCount;
	uint32_t               checkStructureOutputSize;
};

struct IOExternalMethodDispatch2022 {
	IOExternalMethodAction function;
	uint32_t               checkScalarInputCount;
	uint32_t               checkStructureInputSize;
	uint32_t               checkScalarOutputCount;
	uint32_t               checkStructureOutputSize;
	uint8_t                allowAsync;
	const char*            checkEntitlement;
};

enum {
#define IO_EXTERNAL_METHOD_ARGUMENTS_CURRENT_VERSION    2
	kIOExternalMethodArgumentsCurrentVersion = IO_EXTERNAL_METHOD_ARGUMENTS_CURRENT_VERSION
};

#if PRIVATE
typedef uintptr_t io_filter_policy_t __kernel_ptr_semantics;
enum io_filter_type_t {
	io_filter_type_external_method       = 1,
	io_filter_type_external_async_method = 2,
	io_filter_type_trap                  = 3,
};

typedef IOReturn (*io_filter_resolver_t) (task_t task, IOUserClient * client, uint32_t type, io_filter_policy_t *filterp);
typedef IOReturn (*io_filter_applier_t) (IOUserClient * client, io_filter_policy_t filter, io_filter_type_t type, uint32_t selector);
typedef void (*io_filter_release_t) (io_filter_policy_t filter);
struct io_filter_callbacks {
	const io_filter_resolver_t      io_filter_resolver;
	const io_filter_applier_t       io_filter_applier;
	const io_filter_release_t       io_filter_release;
};
struct IOUCFilterPolicy;
#endif /* PRIVATE */

/*!
 *   @class IOUserClient
 *   @abstract   Provides a basis for communication between client applications and I/O Kit objects.
 */
class IOUserClient : public IOService
{
	OSDeclareAbstractStructorsWithDispatch(IOUserClient);
#if IOKITSTATS
	friend class IOStatistics;
#endif

#if XNU_KERNEL_PRIVATE
public:
#else /* XNU_KERNEL_PRIVATE */
protected:
#endif /* !XNU_KERNEL_PRIVATE */
/*! @struct ExpansionData
 *   @discussion This structure will be used to expand the capablilties of this class in the future.
 */
	struct ExpansionData {
#if IOKITSTATS
		IOUserClientCounter *counter;
#else
		void *iokitstatsReserved;
#endif
#if PRIVATE
		IOUCFilterPolicy * filterPolicies;
#else
		void *iokitFilterReserved;
#endif
	};

/*! @var reserved
 *   Reserved for future use.  (Internal use only)
 */
	APPLE_KEXT_WSHADOW_PUSH;
	ExpansionData * reserved;
	APPLE_KEXT_WSHADOW_POP;

	bool reserve();

#ifdef XNU_KERNEL_PRIVATE

public:
	UInt8        __opaque_start[0];

	OSSet * mappings;
	UInt8   sharedInstance;
	UInt8   closed;
	UInt8   __ipcFinal;
	UInt8   messageAppSuspended:1,
	    uc2022:1,
	    defaultLocking:1,
	    defaultLockingSingleThreadExternalMethod:1,
	    defaultLockingSetProperties:1,
	    opened:1,
	    __reservedA:2;
	volatile SInt32 __ipc;
	queue_head_t owners;
	IORWLock     lock;
	IOLock       filterLock;
	void        *__reserved[1];

	UInt8        __opaque_end[0];
#else /* XNU_KERNEL_PRIVATE */
private:
	void  * __reserved[9];
#endif /* XNU_KERNEL_PRIVATE */

public:
	MIG_SERVER_ROUTINE virtual IOReturn
	externalMethod(uint32_t selector, IOExternalMethodArguments * arguments,
	    IOExternalMethodDispatch *dispatch = NULL,
	    OSObject *target = NULL, void *reference = NULL);

	MIG_SERVER_ROUTINE virtual IOReturn registerNotificationPort(
		mach_port_t port, UInt32 type, io_user_reference_t refCon);

private:
	OSMetaClassDeclareReservedUnused(IOUserClient, 0);
	OSMetaClassDeclareReservedUnused(IOUserClient, 1);
	OSMetaClassDeclareReservedUnused(IOUserClient, 2);
	OSMetaClassDeclareReservedUnused(IOUserClient, 3);
	OSMetaClassDeclareReservedUnused(IOUserClient, 4);
	OSMetaClassDeclareReservedUnused(IOUserClient, 5);
	OSMetaClassDeclareReservedUnused(IOUserClient, 6);
	OSMetaClassDeclareReservedUnused(IOUserClient, 7);
	OSMetaClassDeclareReservedUnused(IOUserClient, 8);
	OSMetaClassDeclareReservedUnused(IOUserClient, 9);
	OSMetaClassDeclareReservedUnused(IOUserClient, 10);
	OSMetaClassDeclareReservedUnused(IOUserClient, 11);
	OSMetaClassDeclareReservedUnused(IOUserClient, 12);
	OSMetaClassDeclareReservedUnused(IOUserClient, 13);
	OSMetaClassDeclareReservedUnused(IOUserClient, 14);
	OSMetaClassDeclareReservedUnused(IOUserClient, 15);

#ifdef XNU_KERNEL_PRIVATE

/* Available within xnu source only */
public:
	static void initialize( void );
	static void destroyUserReferences( OSObject * obj );
	static bool finalizeUserReferences( OSObject * obj );
	OSPtr<IOMemoryMap>  mapClientMemory64( IOOptionBits type,
	    task_t task,
	    IOOptionBits mapFlags = kIOMapAnywhere,
	    mach_vm_address_t atAddress = 0 );
	IOReturn registerOwner(task_t task);
	void     noMoreSenders(void);
	io_filter_policy_t filterForTask(task_t task, io_filter_policy_t addFilterPolicy);
	MIG_SERVER_ROUTINE IOReturn
	callExternalMethod(uint32_t selector, IOExternalMethodArguments * arguments);

#endif /* XNU_KERNEL_PRIVATE */

#if PRIVATE
public:
	static IOReturn registerFilterCallbacks(const struct io_filter_callbacks *callbacks, size_t size);
#endif /* PRIVATE */

protected:
	static IOReturn sendAsyncResult(OSAsyncReference reference,
	    IOReturn result, void *args[], UInt32 numArgs);
	static void setAsyncReference(OSAsyncReference asyncRef,
	    mach_port_t wakePort,
	    void *callback, void *refcon);

	static IOReturn sendAsyncResult64(OSAsyncReference64 reference,
	    IOReturn result, io_user_reference_t args[], UInt32 numArgs);

/*!
 *   @function sendAsyncResult64WithOptions
 *   @abstract Send a notification as with sendAsyncResult, but with finite queueing.
 *   @discussion IOUserClient::sendAsyncResult64() will infitely queue messages if the client
 *           is not processing them in a timely fashion.  This variant will not, for simple
 *           handling of situations where clients may be expected to stop processing messages.
 */
	static IOReturn sendAsyncResult64WithOptions(OSAsyncReference64 reference,
	    IOReturn result, io_user_reference_t args[], UInt32 numArgs,
	    IOOptionBits options);

	static void setAsyncReference64(OSAsyncReference64 asyncRef,
	    mach_port_t wakePort,
	    mach_vm_address_t callback, io_user_reference_t refcon);

	static void setAsyncReference64(OSAsyncReference64 asyncRef,
	    mach_port_t wakePort,
	    mach_vm_address_t callback, io_user_reference_t refcon,
	    task_t task);

public:

	static IOReturn clientHasAuthorization( task_t task,
	    IOService * service );

	static IOReturn clientHasPrivilege( void * securityToken,
	    const char * privilegeName );

	static OSPtr<OSObject>  copyClientEntitlement(task_t task, const char *entitlement);
	static OSPtr<OSObject>  copyClientEntitlementVnode(struct vnode *vnode, off_t offset, const char *entitlement);

	static OSPtr<OSDictionary>  copyClientEntitlements(task_t task);
	static OSPtr<OSDictionary>  copyClientEntitlementsVnode(struct vnode *vnode, off_t offset);

/*!
 *   @function releaseAsyncReference64
 *   @abstract Release the mach_port_t reference held within the OSAsyncReference64 structure.
 *   @discussion The OSAsyncReference64 structure passed to async methods holds a reference to the wakeup mach port, which should be released to balance each async method call. Behavior is undefined if these calls are not correctly balanced.
 *   @param reference The reference passed to the subclass IOAsyncMethod, or externalMethod() in the IOExternalMethodArguments.asyncReference field.
 *   @result A return code.
 */
	static IOReturn releaseAsyncReference64(OSAsyncReference64 reference);
/*!
 *   @function releaseNotificationPort
 *   @abstract Release the mach_port_t passed to registerNotificationPort().
 *   @discussion The mach_port_t passed to the registerNotificationPort() methods should be released to balance each call to registerNotificationPort(). Behavior is undefined if these calls are not correctly balanced.
 *   @param port The mach_port_t argument previously passed to the subclass implementation of registerNotificationPort().
 *   @result A return code.
 */
	static IOReturn releaseNotificationPort(mach_port_t port);

	virtual bool init() APPLE_KEXT_OVERRIDE;
	virtual bool init( OSDictionary * dictionary ) APPLE_KEXT_OVERRIDE;
// Currently ignores the all args, just passes up to IOService::init()
	virtual bool initWithTask(
		task_t owningTask, void * securityToken, UInt32 type,
		OSDictionary * properties);

	virtual bool initWithTask(
		task_t owningTask, void * securityToken, UInt32 type);

	virtual void free() APPLE_KEXT_OVERRIDE;

	virtual IOReturn clientClose( void );
	virtual IOReturn clientDied( void );

	virtual IOService * getService( void );

	MIG_SERVER_ROUTINE virtual IOReturn registerNotificationPort(
		mach_port_t port, UInt32 type, UInt32 refCon );

	MIG_SERVER_ROUTINE virtual IOReturn getNotificationSemaphore( UInt32 notification_type,
	    semaphore_t * semaphore );

	virtual IOReturn connectClient( IOUserClient * client );

// memory will be released by user client when last map is destroyed
	virtual IOReturn clientMemoryForType( UInt32 type,
	    IOOptionBits * options,
	    IOMemoryDescriptor ** memory );

	IOReturn clientMemoryForType( UInt32 type,
	    IOOptionBits * options,
	    OSSharedPtr<IOMemoryDescriptor>& memory );

#if !__LP64__
private:
	APPLE_KEXT_COMPATIBILITY_VIRTUAL
	OSPtr<IOMemoryMap>  mapClientMemory( IOOptionBits type,
	    task_t task,
	    IOOptionBits mapFlags = kIOMapAnywhere,
	    IOVirtualAddress atAddress = 0 );
#endif

	static IOReturn _sendAsyncResult64(OSAsyncReference64 reference,
	    IOReturn result, io_user_reference_t args[], UInt32 numArgs, IOOptionBits options);
public:

/*!
 *   @function removeMappingForDescriptor
 *   Remove the first mapping created from the memory descriptor returned by clientMemoryForType() from IOUserClient's list of mappings. If such a mapping exists, it is retained and the reference currently held by IOUserClient is returned to the caller.
 *   @param memory The memory descriptor instance previously returned by the implementation of clientMemoryForType().
 *   @result A reference to the first IOMemoryMap instance found in the list of mappings created by IOUserClient from that passed memory descriptor is returned, or zero if none exist. The caller should release this reference.
 */
	OSPtr<IOMemoryMap>  removeMappingForDescriptor(IOMemoryDescriptor * memory);

/*!
 *   @function exportObjectToClient
 *   Make an arbitrary OSObject available to the client task.
 *   @param task The task.
 *   @param obj The object we want to export to the client.
 *   @param clientObj Returned value is the client's port name.
 */
	virtual IOReturn exportObjectToClient(task_t task,
	    LIBKERN_CONSUMED OSObject *obj, io_object_t *clientObj);

#if KERNEL_PRIVATE

/*!
 *   @function copyPortNameForObjectInTask
 *   Make an arbitrary OSObject available to the client task as a port name.
 *   The port does not respond to any IOKit IPC calls.
 *   @param task The task.
 *   @param object The object we want to export to the client.
 *   The port holds a reference on the object, this function does not consume any reference on the object.
 *   @param port_name Returned value is the task's port name. It has one send right created by this function.
 *   @result A return code.
 */
	static IOReturn copyPortNameForObjectInTask(task_t task, OSObject *object,
	    mach_port_name_t * port_name);

/*!
 *   @function copyObjectForPortNameInTask
 *   Look up an OSObject given a task's port name created with copyPortNameForObjectInTask().
 *   @param task The task.
 *   @param port_name The task's port name. This function does not consume any reference on the port name.
 *   @param object If the port name is valid, a reference to the object is returned. It should be released by the caller.
 *   @result A return code.
 */
	static IOReturn copyObjectForPortNameInTask(task_t task, mach_port_name_t port_name,
	    OSObject **object);

	static IOReturn copyObjectForPortNameInTask(task_t task, mach_port_name_t port_name,
	    OSSharedPtr<OSObject>& object);

/*!
 *   @function adjustPortNameReferencesInTask
 *   Adjust the send rights for a port name created with copyPortNameForObjectInTask().
 *   @param task The task.
 *   @param port_name The task's port name.
 *   @param delta Signed value change to the number of user references.
 *   @result A return code.
 */
	static IOReturn adjustPortNameReferencesInTask(task_t task, mach_port_name_t port_name, mach_port_delta_t delta);

#define IOUC_COPYPORTNAMEFOROBJECTINTASK    1

#endif /* KERNEL_PRIVATE */

// Old methods for accessing method vector backward compatiblility only
	virtual IOExternalMethod *
	getExternalMethodForIndex( UInt32 index )
	APPLE_KEXT_DEPRECATED;
	virtual IOExternalAsyncMethod *
	getExternalAsyncMethodForIndex( UInt32 index )
	APPLE_KEXT_DEPRECATED;

// Methods for accessing method vector.
	virtual IOExternalMethod *
	getTargetAndMethodForIndex(
		LIBKERN_RETURNS_NOT_RETAINED IOService ** targetP, UInt32 index );
	virtual IOExternalAsyncMethod *
	getAsyncTargetAndMethodForIndex(
		LIBKERN_RETURNS_NOT_RETAINED IOService ** targetP, UInt32 index );
	IOExternalMethod *
	getTargetAndMethodForIndex(
		OSSharedPtr<IOService>& targetP, UInt32 index );
	IOExternalAsyncMethod *
	getAsyncTargetAndMethodForIndex(
		OSSharedPtr<IOService>& targetP, UInt32 index );

// Methods for accessing trap vector - old and new style
	virtual IOExternalTrap *
	getExternalTrapForIndex( UInt32 index )
	APPLE_KEXT_DEPRECATED;

	virtual IOExternalTrap *
	getTargetAndTrapForIndex(
		LIBKERN_RETURNS_NOT_RETAINED IOService **targetP, UInt32 index );
};

#if KERNEL_PRIVATE

#define IOUSERCLIENT2022_SUPPORTED      1

/*
 *  IOUserClient2022 is a new superclass for an IOUserClient implementation to opt-in to
 *  several security related best practices. The changes in behavior are:
 *  - these properties must be present after ::start completes to control default single
 *  threading calls to the IOUC from clients. It is recommended to set all values to true.
 *
 *  kIOUserClientDefaultLockingKey if kOSBooleanTrue
 *  IOConnectMapMemory, IOConnectUnmapMemory, IOConnectAddClient, IOServiceClose
 *  are single threaded and will not allow externalMethod to run concurrently.
 *  Multiple threads can call externalMethod concurrently however.
 *
 *  kIOUserClientDefaultLockingSetPropertiesKey if kOSBooleanTrue
 *  IORegistrySetProperties is also single threaded as above.
 *
 *  kIOUserClientDefaultLockingSingleThreadExternalMethodKey if kOSBooleanTrue
 *  Only one thread may call externalMethod concurrently.
 *
 *  kIOUserClientEntitlementsKey
 *  Entitlements required for a process to open the IOUC (see the key description).
 *  It is recommended to require an entitlement if all calling processes are from Apple.
 *
 *  - the externalMethod override is required and must call dispatchExternalMethod() to
 *  do basic argument checking before calling subclass code to implement the method.
 *  dispatchExternalMethod() is called with an array of IOExternalMethodDispatch2022
 *  elements and will index into the array for the given selector. The selector should
 *  be adjusted accordingly, if needed, in the subclass' externalMethod().
 *  The allowAsync field of IOExternalMethodDispatch2022 must be true to allow the
 *  IOConnectCallAsyncMethod(etc) APIs to be used with the method.
 *  If the checkEntitlement field of IOExternalMethodDispatch2022 is non-NULL, then
 *  the calling process must have the named entitlement key, with a value of boolean true,
 *  or kIOReturnNotPrivileged is returned. This should be used when per-selector entitlement
 *  checks are required.  If you only need to check at the time the connection is created,
 *  use kIOUserClientEntitlementsKey instead.
 */

class IOUserClient2022 : public IOUserClient
{
	OSDeclareDefaultStructors(IOUserClient2022);

private:
	MIG_SERVER_ROUTINE virtual IOReturn
	externalMethod(uint32_t selector, IOExternalMethodArguments * arguments,
	    IOExternalMethodDispatch *dispatch = NULL,
	    OSObject *target = NULL, void *reference = NULL) APPLE_KEXT_OVERRIDE;

protected:
	IOReturn
	dispatchExternalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * arguments,
	    const IOExternalMethodDispatch2022 dispatchArray[], size_t dispatchArrayCount,
	    OSObject * target, void * reference);

public:

	MIG_SERVER_ROUTINE virtual IOReturn
	externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * arguments) = 0;


	OSMetaClassDeclareReservedUnused(IOUserClient2022, 0);
	OSMetaClassDeclareReservedUnused(IOUserClient2022, 1);
	OSMetaClassDeclareReservedUnused(IOUserClient2022, 2);
	OSMetaClassDeclareReservedUnused(IOUserClient2022, 3);
};

#endif /* KERNEL_PRIVATE */

#ifdef XNU_KERNEL_PRIVATE

class IOUserIterator : public OSIterator
{
	OSDeclareDefaultStructors(IOUserIterator);
public:
	OSObject    *       userIteratorObject;
	IOLock              lock;

	static IOUserIterator * withIterator(LIBKERN_CONSUMED OSIterator * iter);
	virtual bool init( void ) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;

	virtual void reset() APPLE_KEXT_OVERRIDE;
	virtual bool isValid() APPLE_KEXT_OVERRIDE;
	virtual OSObject * getNextObject() APPLE_KEXT_OVERRIDE;
	virtual OSObject * copyNextObject();
};

class IOUserNotification : public IOUserIterator
{
	OSDeclareDefaultStructors(IOUserNotification);

public:

	virtual void free() APPLE_KEXT_OVERRIDE;

	virtual void setNotification( IONotifier * obj );

	virtual void reset() APPLE_KEXT_OVERRIDE;
	virtual bool isValid() APPLE_KEXT_OVERRIDE;
};

#endif /* XNU_KERNEL_PRIVATE */

#endif /* ! _IOKIT_IOUSERCLIENT_H */
