/*
 * Copyright (c) 1998-2022 Apple Inc. All rights reserved.
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

#include <IOKit/IOCPU.h>
#include <IOKit/IOPlatformActions.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitDebug.h>
#include <IOKit/IOMapper.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IONVRAM.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOTimeStamp.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitDiagnosticsUserClient.h>
#include <IOKit/IOUserServer.h>

#include "IOKitKernelInternal.h"

#include <IOKit/system.h>
#include <sys/csr.h>

#include <libkern/c++/OSContainers.h>
#include <libkern/c++/OSSharedPtr.h>
#include <libkern/crypto/sha1.h>
#include <libkern/OSAtomic.h>

#if defined(__arm64__)
#include <arm64/tlb.h>
#endif

extern "C" {
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include <uuid/uuid.h>
#include <sys/sysctl.h>
}

#define kShutdownTimeout    30 //in secs

#if defined(XNU_TARGET_OS_OSX)

boolean_t coprocessor_cross_panic_enabled = TRUE;
#define APPLE_VENDOR_VARIABLE_GUID "4d1ede05-38c7-4a6a-9cc6-4bcca8b38c14"
#endif /* defined(XNU_TARGET_OS_OSX) */

void printDictionaryKeys(OSDictionary * inDictionary, char * inMsg);
static void getCStringForObject(OSObject *inObj, char *outStr, size_t outStrLen);

/*
 * There are drivers which take mutexes in the quiesce callout or pass
 * the quiesce/active action to super.  Even though it sometimes panics,
 * because it doesn't *always* panic, they get away with it.
 * We need a chicken bit to diagnose and fix them all before this
 * can be enabled by default.
 *
 * <rdar://problem/33831837> tracks turning this on by default.
 */
uint32_t gEnforcePlatformActionSafety = 0;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IOService

OSDefineMetaClassAndStructors(IOPlatformExpert, IOService)

OSMetaClassDefineReservedUsedX86(IOPlatformExpert, 0);
OSMetaClassDefineReservedUsedX86(IOPlatformExpert, 1);
OSMetaClassDefineReservedUsedX86(IOPlatformExpert, 2);
OSMetaClassDefineReservedUsedX86(IOPlatformExpert, 3);
OSMetaClassDefineReservedUsedX86(IOPlatformExpert, 4);
OSMetaClassDefineReservedUsedX86(IOPlatformExpert, 5);
OSMetaClassDefineReservedUsedX86(IOPlatformExpert, 6);

OSMetaClassDefineReservedUnused(IOPlatformExpert, 7);
OSMetaClassDefineReservedUnused(IOPlatformExpert, 8);
OSMetaClassDefineReservedUnused(IOPlatformExpert, 9);
OSMetaClassDefineReservedUnused(IOPlatformExpert, 10);
OSMetaClassDefineReservedUnused(IOPlatformExpert, 11);

static IOPlatformExpert * gIOPlatform;
static OSDictionary * gIOInterruptControllers;
static IOLock * gIOInterruptControllersLock;
static IODTNVRAM *gIOOptionsEntry;

OSSymbol * gPlatformInterruptControllerName;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool
IOPlatformExpert::attach( IOService * provider )
{
	if (!super::attach( provider )) {
		return false;
	}

	return true;
}

bool
IOPlatformExpert::start( IOService * provider )
{
	IORangeAllocator *  physicalRanges;
	OSData *            busFrequency;
	uint32_t            debugFlags;


	if (!super::start(provider)) {
		return false;
	}

	// Override the mapper present flag is requested by boot arguments, if SIP disabled.
#if CONFIG_CSR
	if (csr_check(CSR_ALLOW_UNRESTRICTED_FS) == 0)
#endif /* CONFIG_CSR */
	{
		if (PE_parse_boot_argn("dart", &debugFlags, sizeof(debugFlags)) && (debugFlags == 0)) {
			removeProperty(kIOPlatformMapperPresentKey);
		}
#if DEBUG || DEVELOPMENT
		if (PE_parse_boot_argn("-x", &debugFlags, sizeof(debugFlags))) {
			removeProperty(kIOPlatformMapperPresentKey);
		}
#endif /* DEBUG || DEVELOPMENT */
	}

	// Register the presence or lack thereof a system
	// PCI address mapper with the IOMapper class
	IOMapper::setMapperRequired(NULL != getProperty(kIOPlatformMapperPresentKey));

	gIOInterruptControllers = OSDictionary::withCapacity(1);
	gIOInterruptControllersLock = IOLockAlloc();

	// Correct the bus frequency in the device tree.
	busFrequency = OSData::withBytesNoCopy((void *)&gPEClockFrequencyInfo.bus_clock_rate_hz, 4);
	provider->setProperty("clock-frequency", busFrequency);
	busFrequency->release();

	gPlatformInterruptControllerName = (OSSymbol *)OSSymbol::withCStringNoCopy("IOPlatformInterruptController");

	physicalRanges = IORangeAllocator::withRange(0xffffffff, 1, 16,
	    IORangeAllocator::kLocking);
	assert(physicalRanges);
	setProperty("Platform Memory Ranges", physicalRanges);
	OSSafeReleaseNULL(physicalRanges);

	setPlatform( this );
	gIOPlatform = this;

	PMInstantiatePowerDomains();

#if !defined(__x86_64__)
	publishPlatformUUIDAndSerial();
#endif /* !defined(__x86_64__) */

#if defined (__x86_64__)
	if (PEGetCoprocessorVersion() >= kCoprocessorVersion2) {
		coprocessor_paniclog_flush = TRUE;
		extended_debug_log_init();
	}
#endif

	PE_parse_boot_argn("enforce_platform_action_safety", &gEnforcePlatformActionSafety,
	    sizeof(gEnforcePlatformActionSafety));

	return configure(provider);
}

bool
IOPlatformExpert::configure( IOService * provider )
{
	OSSet *             topLevel;
	OSDictionary *      dict;
	IOService *         nub = NULL;

	topLevel = OSDynamicCast( OSSet, getProperty("top-level"));

	if (topLevel) {
		while ((dict = OSDynamicCast( OSDictionary,
		    topLevel->getAnyObject()))) {
			dict->retain();
			topLevel->removeObject( dict );
			OSSafeReleaseNULL(nub);
			nub = createNub( dict );
			dict->release();
			if (NULL == nub) {
				continue;
			}
			nub->attach( this );
			nub->registerService();
		}
	}
	OSSafeReleaseNULL(nub);
	return true;
}

IOService *
IOPlatformExpert::createNub( OSDictionary * from )
{
	IOService *         nub;

	nub = new IOPlatformDevice;
	if (nub) {
		if (!nub->init( from )) {
			nub->release();
			nub = NULL;
		}
	}
	return nub;
}

bool
IOPlatformExpert::compareNubName( const IOService * nub,
    OSString * name, OSString ** matched ) const
{
	return nub->IORegistryEntry::compareName( name, matched );
}

bool
IOPlatformExpert::compareNubName( const IOService * nub,
    OSString * name, OSSharedPtr<OSString>& matched ) const
{
	OSString* matchedRaw = NULL;
	bool result = compareNubName(nub, name, &matchedRaw);
	matched.reset(matchedRaw, OSNoRetain);
	return result;
}

IOReturn
IOPlatformExpert::getNubResources( IOService * nub )
{
	return kIOReturnSuccess;
}

long
IOPlatformExpert::getBootROMType(void)
{
	return _peBootROMType;
}

long
IOPlatformExpert::getChipSetType(void)
{
	return _peChipSetType;
}

long
IOPlatformExpert::getMachineType(void)
{
	return _peMachineType;
}

void
IOPlatformExpert::setBootROMType(long peBootROMType)
{
	_peBootROMType = peBootROMType;
}

void
IOPlatformExpert::setChipSetType(long peChipSetType)
{
	_peChipSetType = peChipSetType;
}

void
IOPlatformExpert::setMachineType(long peMachineType)
{
	_peMachineType = peMachineType;
}

bool
IOPlatformExpert::getMachineName( char * /*name*/, int /*maxLength*/)
{
	return false;
}

bool
IOPlatformExpert::getModelName( char * /*name*/, int /*maxLength*/)
{
	return false;
}

bool
IOPlatformExpert::getTargetName( char * /*name*/, int /*maxLength*/)
{
	return false;
}

bool
IOPlatformExpert::getProductName( char * /*name*/, int /*maxLength*/)
{
	return false;
}

OSString*
IOPlatformExpert::createSystemSerialNumberString(OSData* myProperty)
{
	return NULL;
}

IORangeAllocator *
IOPlatformExpert::getPhysicalRangeAllocator(void)
{
	return OSDynamicCast(IORangeAllocator,
	           getProperty("Platform Memory Ranges"));
}

int (*PE_halt_restart)(unsigned int type) = NULL;

int
IOPlatformExpert::haltRestart(unsigned int type)
{
	if (type == kPEPanicSync) {
		return 0;
	}

	if (type == kPEHangCPU) {
		while (true) {
			asm volatile ("");
		}
	}

	if (type == kPEUPSDelayHaltCPU) {
		// RestartOnPowerLoss feature was turned on, proceed with shutdown.
		type = kPEHaltCPU;
	}

#if defined (__x86_64__)
	// On ARM kPEPanicRestartCPU is supported in the drivers
	if (type == kPEPanicRestartCPU) {
		type = kPERestartCPU;
	}
#endif

	if (PE_halt_restart) {
		return (*PE_halt_restart)(type);
	} else {
		return -1;
	}
}

void
IOPlatformExpert::sleepKernel(void)
{
#if 0
	long cnt;
	boolean_t intState;

	intState = ml_set_interrupts_enabled(false);

	for (cnt = 0; cnt < 10000; cnt++) {
		IODelay(1000);
	}

	ml_set_interrupts_enabled(intState);
#else
//  PE_initialize_console(0, kPEDisableScreen);

	IOCPUSleepKernel();

//  PE_initialize_console(0, kPEEnableScreen);
#endif
}

long
IOPlatformExpert::getGMTTimeOfDay(void)
{
	return 0;
}

void
IOPlatformExpert::setGMTTimeOfDay(long secs)
{
}


IOReturn
IOPlatformExpert::getConsoleInfo( PE_Video * consoleInfo )
{
	return PE_current_console( consoleInfo);
}

IOReturn
IOPlatformExpert::setConsoleInfo( PE_Video * consoleInfo,
    unsigned int op)
{
	return PE_initialize_console( consoleInfo, op );
}

IOReturn
IOPlatformExpert::registerInterruptController(OSSymbol *name, IOInterruptController *interruptController)
{
	IOLockLock(gIOInterruptControllersLock);

	gIOInterruptControllers->setObject(name, interruptController);

	IOLockWakeup(gIOInterruptControllersLock,
	    gIOInterruptControllers, /* one-thread */ false);

	IOLockUnlock(gIOInterruptControllersLock);

	return kIOReturnSuccess;
}

IOReturn
IOPlatformExpert::deregisterInterruptController(OSSymbol *name)
{
	IOLockLock(gIOInterruptControllersLock);

	gIOInterruptControllers->removeObject(name);

	IOLockUnlock(gIOInterruptControllersLock);

	return kIOReturnSuccess;
}

IOInterruptController *
IOPlatformExpert::lookUpInterruptController(OSSymbol *name)
{
	OSObject              *object;

	IOLockLock(gIOInterruptControllersLock);
	while (1) {
		object = gIOInterruptControllers->getObject(name);

		if (object != NULL) {
			break;
		}

		IOLockSleep(gIOInterruptControllersLock,
		    gIOInterruptControllers, THREAD_UNINT);
	}

	IOLockUnlock(gIOInterruptControllersLock);
	return OSDynamicCast(IOInterruptController, object);
}


void
IOPlatformExpert::setCPUInterruptProperties(IOService *service)
{
	IOInterruptController *controller;

	OSDictionary *matching = serviceMatching("IOInterruptController");
	matching = propertyMatching(gPlatformInterruptControllerName, kOSBooleanTrue, matching);

	controller = OSDynamicCast(IOInterruptController, waitForService(matching));
	if (controller) {
		controller->setCPUInterruptProperties(service);
	}
}

bool
IOPlatformExpert::atInterruptLevel(void)
{
	return ml_at_interrupt_context();
}

bool
IOPlatformExpert::platformAdjustService(IOService */*service*/)
{
	return true;
}

void
IOPlatformExpert::getUTCTimeOfDay(clock_sec_t * secs, clock_nsec_t * nsecs)
{
	*secs = getGMTTimeOfDay();
	*nsecs = 0;
}

void
IOPlatformExpert::setUTCTimeOfDay(clock_sec_t secs, __unused clock_nsec_t nsecs)
{
	setGMTTimeOfDay(secs);
}


//*********************************************************************************
// PMLog
//
//*********************************************************************************

void
IOPlatformExpert::
PMLog(const char *who, unsigned long event,
    unsigned long param1, unsigned long param2)
{
	clock_sec_t nows;
	clock_usec_t nowus;
	clock_get_system_microtime(&nows, &nowus);
	nowus += (nows % 1000) * 1000000;

	kprintf("pm%u %p %.30s %d %lx %lx\n",
	    nowus, OBFUSCATE(current_thread()), who,            // Identity
	    (int) event, (long)OBFUSCATE(param1), (long)OBFUSCATE(param2));                     // Args
}


//*********************************************************************************
// PMInstantiatePowerDomains
//
// In this vanilla implementation, a Root Power Domain is instantiated.
// All other objects which register will be children of this Root.
// Where this is inappropriate, PMInstantiatePowerDomains is overridden
// in a platform-specific subclass.
//*********************************************************************************

void
IOPlatformExpert::PMInstantiatePowerDomains( void )
{
	root = new IOPMrootDomain;
	root->init();
	root->attach(this);
	root->start(this);
}


//*********************************************************************************
// PMRegisterDevice
//
// In this vanilla implementation, all callers are made children of the root power domain.
// Where this is inappropriate, PMRegisterDevice is overridden in a platform-specific subclass.
//*********************************************************************************

void
IOPlatformExpert::PMRegisterDevice(IOService * theNub, IOService * theDevice)
{
	root->addPowerChild( theDevice );
}

//*********************************************************************************
// hasPMFeature
//
//*********************************************************************************

bool
IOPlatformExpert::hasPMFeature(unsigned long featureMask)
{
	return (_pePMFeatures & featureMask) != 0;
}

//*********************************************************************************
// hasPrivPMFeature
//
//*********************************************************************************

bool
IOPlatformExpert::hasPrivPMFeature(unsigned long privFeatureMask)
{
	return (_pePrivPMFeatures & privFeatureMask) != 0;
}

//*********************************************************************************
// numBatteriesSupported
//
//*********************************************************************************

int
IOPlatformExpert::numBatteriesSupported(void)
{
	return _peNumBatteriesSupported;
}

//*********************************************************************************
// CheckSubTree
//
// This method is called by the instantiated sublass of the platform expert to
// determine how a device should be inserted into the Power Domain. The subclass
// provides an XML power tree description against which a device is matched based
// on class and provider. If a match is found this routine returns true in addition
// to flagging the description tree at the appropriate node that a device has been
// registered for the given service.
//*********************************************************************************

bool
IOPlatformExpert::CheckSubTree(OSArray * inSubTree, IOService * theNub, IOService * theDevice, OSDictionary * theParent)
{
	unsigned int    i;
	unsigned int    numPowerTreeNodes;
	OSDictionary *  entry;
	OSDictionary *  matchingDictionary;
	OSDictionary *  providerDictionary;
	OSDictionary *  deviceDictionary;
	OSDictionary *  nubDictionary;
	OSArray *       children;
	bool            nodeFound            = false;
	bool            continueSearch       = false;
	bool            deviceMatch          = false;
	bool            providerMatch        = false;
	bool            multiParentMatch     = false;

	if ((NULL == theDevice) || (NULL == inSubTree)) {
		return false;
	}

	numPowerTreeNodes = inSubTree->getCount();

	// iterate through the power tree to find a home for this device

	for (i = 0; i < numPowerTreeNodes; i++) {
		entry =  (OSDictionary *) inSubTree->getObject(i);

		matchingDictionary = (OSDictionary *) entry->getObject("device");
		providerDictionary = (OSDictionary *) entry->getObject("provider");

		deviceMatch = true; // if no matching dictionary, this is not a criteria and so must match
		if (matchingDictionary) {
			deviceMatch = false;
			if (NULL != (deviceDictionary = theDevice->dictionaryWithProperties())) {
				deviceMatch = deviceDictionary->isEqualTo( matchingDictionary, matchingDictionary );
				deviceDictionary->release();
			}
		}

		providerMatch = true; // we indicate a match if there is no nub or provider
		if (theNub && providerDictionary) {
			providerMatch = false;
			if (NULL != (nubDictionary = theNub->dictionaryWithProperties())) {
				providerMatch = nubDictionary->isEqualTo( providerDictionary, providerDictionary );
				nubDictionary->release();
			}
		}

		multiParentMatch = true; // again we indicate a match if there is no multi-parent node
		if (deviceMatch && providerMatch) {
			if (NULL != multipleParentKeyValue) {
				OSNumber * aNumber = (OSNumber *) entry->getObject("multiple-parent");
				multiParentMatch   = (NULL != aNumber) ? multipleParentKeyValue->isEqualTo(aNumber) : false;
			}
		}

		nodeFound = (deviceMatch && providerMatch && multiParentMatch);

		// if the power tree specifies a provider dictionary but theNub is
		// NULL then we cannot match with this entry.

		if (theNub == NULL && providerDictionary != NULL) {
			nodeFound = false;
		}

		// if this node is THE ONE...then register the device

		if (nodeFound) {
			if (RegisterServiceInTree(theDevice, entry, theParent, theNub)) {
				if (kIOLogPower & gIOKitDebug) {
					IOLog("PMRegisterDevice/CheckSubTree - service registered!\n");
				}

				numInstancesRegistered++;

				// determine if we need to search for additional nodes for this item
				multipleParentKeyValue = (OSNumber *) entry->getObject("multiple-parent");
			} else {
				nodeFound = false;
			}
		}

		continueSearch = ((false == nodeFound) || (NULL != multipleParentKeyValue));

		if (continueSearch && (NULL != (children = (OSArray *) entry->getObject("children")))) {
			nodeFound = CheckSubTree( children, theNub, theDevice, entry );
			continueSearch = ((false == nodeFound) || (NULL != multipleParentKeyValue));
		}

		if (false == continueSearch) {
			break;
		}
	}

	return nodeFound;
}

//*********************************************************************************
// RegisterServiceInTree
//
// Register a device at the specified node of our power tree.
//*********************************************************************************

bool
IOPlatformExpert::RegisterServiceInTree(IOService * theService, OSDictionary * theTreeNode, OSDictionary * theTreeParentNode, IOService * theProvider)
{
	IOService *    aService;
	bool           registered = false;
	OSArray *      children;
	unsigned int   numChildren;
	OSDictionary * child;

	// make sure someone is not already registered here

	if (NULL == theTreeNode->getObject("service")) {
		if (theTreeNode->setObject("service", OSDynamicCast( OSObject, theService))) {
			// 1. CHILDREN ------------------

			// we registered the node in the tree...now if the node has children
			// registered we must tell this service to add them.

			if (NULL != (children = (OSArray *) theTreeNode->getObject("children"))) {
				numChildren = children->getCount();
				for (unsigned int i = 0; i < numChildren; i++) {
					if (NULL != (child = (OSDictionary *) children->getObject(i))) {
						if (NULL != (aService = (IOService *) child->getObject("service"))) {
							theService->addPowerChild(aService);
						}
					}
				}
			}

			// 2. PARENT --------------------

			// also we must notify the parent of this node (if a registered service
			// exists there) of a new child.

			if (theTreeParentNode) {
				if (NULL != (aService = (IOService *) theTreeParentNode->getObject("service"))) {
					if (aService != theProvider) {
						aService->addPowerChild(theService);
					}
				}
			}

			registered = true;
		}
	}

	return registered;
}

//*********************************************************************************
// printDictionaryKeys
//
// Print the keys for the given dictionary and selected contents.
//*********************************************************************************
void
printDictionaryKeys(OSDictionary * inDictionary, char * inMsg)
{
	OSCollectionIterator * mcoll = OSCollectionIterator::withCollection(inDictionary);
	OSSymbol * mkey;
	OSString * ioClass;
	unsigned int i = 0;

	mcoll->reset();

	mkey = OSDynamicCast(OSSymbol, mcoll->getNextObject());

	while (mkey) {
		// kprintf ("dictionary key #%d: %s\n", i, mkey->getCStringNoCopy () );

		// if this is the IOClass key, print it's contents

		if (mkey->isEqualTo("IOClass")) {
			ioClass = (OSString *) inDictionary->getObject("IOClass");
			if (ioClass) {
				IOLog("%s IOClass is %s\n", inMsg, ioClass->getCStringNoCopy());
			}
		}

		// if this is an IOProviderClass key print it

		if (mkey->isEqualTo("IOProviderClass")) {
			ioClass = (OSString *) inDictionary->getObject("IOProviderClass");
			if (ioClass) {
				IOLog("%s IOProviderClass is %s\n", inMsg, ioClass->getCStringNoCopy());
			}
		}

		// also print IONameMatch keys
		if (mkey->isEqualTo("IONameMatch")) {
			ioClass = (OSString *) inDictionary->getObject("IONameMatch");
			if (ioClass) {
				IOLog("%s IONameMatch is %s\n", inMsg, ioClass->getCStringNoCopy());
			}
		}

		// also print IONameMatched keys

		if (mkey->isEqualTo("IONameMatched")) {
			ioClass = (OSString *) inDictionary->getObject("IONameMatched");
			if (ioClass) {
				IOLog("%s IONameMatched is %s\n", inMsg, ioClass->getCStringNoCopy());
			}
		}

#if 0
		// print clock-id

		if (mkey->isEqualTo("AAPL,clock-id")) {
			char * cstr;
			cstr = getCStringForObject(inDictionary->getObject("AAPL,clock-id"));
			if (cstr) {
				kprintf(" ===> AAPL,clock-id is %s\n", cstr );
			}
		}
#endif

		// print name

		if (mkey->isEqualTo("name")) {
			char nameStr[64];
			nameStr[0] = 0;
			getCStringForObject(inDictionary->getObject("name"), nameStr,
			    sizeof(nameStr));
			if (strlen(nameStr) > 0) {
				IOLog("%s name is %s\n", inMsg, nameStr);
			}
		}

		mkey = (OSSymbol *) mcoll->getNextObject();

		i++;
	}

	mcoll->release();
}

static void
getCStringForObject(OSObject *inObj, char *outStr, size_t outStrLen)
{
	char * buffer;
	unsigned int    len, i;

	if ((NULL == inObj) || (NULL == outStr)) {
		return;
	}

	char * objString = (char *) (inObj->getMetaClass())->getClassName();

	if ((0 == strncmp(objString, "OSString", sizeof("OSString"))) ||
	    (0 == strncmp(objString, "OSSymbol", sizeof("OSSymbol")))) {
		strlcpy(outStr, ((OSString *)inObj)->getCStringNoCopy(), outStrLen);
	} else if (0 == strncmp(objString, "OSData", sizeof("OSData"))) {
		len = ((OSData *)inObj)->getLength();
		buffer = (char *)((OSData *)inObj)->getBytesNoCopy();
		if (buffer && (len > 0)) {
			for (i = 0; i < len; i++) {
				outStr[i] = buffer[i];
			}
			outStr[len] = 0;
		}
	}
}

/* IOShutdownNotificationsTimedOut
 * - Called from a timer installed by PEHaltRestart
 */
#if !defined(__x86_64)
__abortlike
#endif
static void
IOShutdownNotificationsTimedOut(
	thread_call_param_t p0,
	thread_call_param_t p1)
{
#if !defined(__x86_64__)
	/* 30 seconds has elapsed - panic */
	panic("Halt/Restart Timed Out");

#else /* !defined(__x86_64__) */
	int type = (int)(long)p0;
	uint32_t timeout = (uint32_t)(uintptr_t)p1;

	IOPMrootDomain *pmRootDomain = IOService::getPMRootDomain();
	if (pmRootDomain) {
		if ((PEGetCoprocessorVersion() >= kCoprocessorVersion2) || pmRootDomain->checkShutdownTimeout()) {
			pmRootDomain->panicWithShutdownLog(timeout * 1000);
		}
	}

	/* 30 seconds has elapsed - resume shutdown */
	if (gIOPlatform) {
		gIOPlatform->haltRestart(type);
	}
#endif /* defined(__x86_64__) */
}


extern "C" {
/*
 * Callouts from BSD for machine name & model
 */

/*
 * PEGetMachineName() and PEGetModelName() are inconsistent across
 * architectures, and considered deprecated. Use PEGetTargetName() and
 * PEGetProductName() instead.
 */
boolean_t
PEGetMachineName( char * name, int maxLength )
{
	if (gIOPlatform) {
		return gIOPlatform->getMachineName( name, maxLength );
	} else {
		return false;
	}
}

/*
 * PEGetMachineName() and PEGetModelName() are inconsistent across
 * architectures, and considered deprecated. Use PEGetTargetName() and
 * PEGetProductName() instead.
 */
boolean_t
PEGetModelName( char * name, int maxLength )
{
	if (gIOPlatform) {
		return gIOPlatform->getModelName( name, maxLength );
	} else {
		return false;
	}
}

boolean_t
PEGetTargetName( char * name, int maxLength )
{
	if (gIOPlatform) {
		return gIOPlatform->getTargetName( name, maxLength );
	} else {
		return false;
	}
}

boolean_t
PEGetProductName( char * name, int maxLength )
{
	if (gIOPlatform) {
		return gIOPlatform->getProductName( name, maxLength );
	} else {
		return false;
	}
}

int
PEGetPlatformEpoch(void)
{
	if (gIOPlatform) {
		return (int) gIOPlatform->getBootROMType();
	} else {
		return -1;
	}
}

/* Handle necessary platform specific actions prior to panic */
void
PEInitiatePanic(void)
{
#if defined(__arm64__)
	/*
	 * Trigger a TLB flush so any hard hangs exercise the SoC diagnostic
	 * collection flow rather than hanging late in panic (see rdar://58062030)
	 */
	flush_mmu_tlb_entries_async(0, PAGE_SIZE, PAGE_SIZE, true, true);
	arm64_sync_tlb(true);
#endif // defined(__arm64__)
}

int
PEHaltRestartInternal(unsigned int type, uint32_t details)
{
	IOPMrootDomain    *pmRootDomain;
	AbsoluteTime      deadline;
	thread_call_t     shutdown_hang;
	IORegistryEntry   *node;
	OSData            *data;
	uint32_t          timeout = kShutdownTimeout;
	static boolean_t  panic_begin_called = FALSE;

	if (type == kPEHaltCPU || type == kPERestartCPU || type == kPEUPSDelayHaltCPU) {
		/* If we're in the panic path, the locks and memory allocations required below
		 *  could fail. So just try to reboot instead of risking a nested panic.
		 */
		if (panic_begin_called) {
			goto skip_to_haltRestart;
		}

		pmRootDomain = IOService::getPMRootDomain();
		/* Notify IOKit PM clients of shutdown/restart
		 *  Clients subscribe to this message with a call to
		 *  IOService::registerInterest()
		 */

		/* Spawn a thread that will panic in 30 seconds.
		 *  If all goes well the machine will be off by the time
		 *  the timer expires. If the device wants a different
		 *  timeout, use that value instead of 30 seconds.
		 */
#if  defined(__arm64__)
#define RESTART_NODE_PATH    "/defaults"
#else
#define RESTART_NODE_PATH    "/chosen"
#endif
		node = IORegistryEntry::fromPath( RESTART_NODE_PATH, gIODTPlane );
		if (node) {
			data = OSDynamicCast( OSData, node->getProperty( "halt-restart-timeout" ));
			if (data && data->getLength() == 4) {
				timeout = *((uint32_t *) data->getBytesNoCopy());
			}
			OSSafeReleaseNULL(node);
		}

#if (DEVELOPMENT || DEBUG)
		/* Override the default timeout via a boot-arg */
		uint32_t boot_arg_val;
		if (PE_parse_boot_argn("halt_restart_timeout", &boot_arg_val, sizeof(boot_arg_val))) {
			timeout = boot_arg_val;
		}
#endif

		if (timeout) {
			shutdown_hang = thread_call_allocate( &IOShutdownNotificationsTimedOut,
			    (thread_call_param_t)(uintptr_t) type);
			clock_interval_to_deadline( timeout, kSecondScale, &deadline );
			thread_call_enter1_delayed( shutdown_hang, (thread_call_param_t)(uintptr_t)timeout, deadline );
		}

		pmRootDomain->handlePlatformHaltRestart(type);
		/* This notification should have few clients who all do
		 *  their work synchronously.
		 *
		 *  In this "shutdown notification" context we don't give
		 *  drivers the option of working asynchronously and responding
		 *  later. PM internals make it very hard to wait for asynchronous
		 *  replies.
		 */
	} else if (type == kPEPanicRestartCPU || type == kPEPanicSync || type == kPEPanicRestartCPUNoCallouts) {
		if (type == kPEPanicRestartCPU) {
			// Notify any listeners that we're done collecting
			// panic data before we call through to do the restart
#if defined(__x86_64__)
			if (coprocessor_cross_panic_enabled)
#endif
			IOCPURunPlatformPanicActions(kPEPanicEnd, details);
		} else if (type == kPEPanicRestartCPUNoCallouts) {
			// We skipped the callouts so now set the type to
			// the variant that the platform uses for panic restarts.
			type = kPEPanicRestartCPU;
		}


		// Do an initial sync to flush as much panic data as possible,
		// in case we have a problem in one of the platorm panic handlers.
		// After running the platform handlers, do a final sync w/
		// platform hardware quiesced for the panic.
		PE_sync_panic_buffers();
		IOCPURunPlatformPanicActions(type, details);
		PE_sync_panic_buffers();
	} else if (type == kPEPanicEnd) {
#if defined(__x86_64__)
		if (coprocessor_cross_panic_enabled)
#endif
		IOCPURunPlatformPanicActions(type, details);
	} else if (type == kPEPanicBegin) {
#if defined(__x86_64__)
		if (coprocessor_cross_panic_enabled)
#endif
		{
			// Only call the kPEPanicBegin callout once
			if (!panic_begin_called) {
				panic_begin_called = TRUE;
				IOCPURunPlatformPanicActions(type, details);
			}
		}
	} else if (type == kPEPanicDiagnosticsDone || type == kPEPanicDiagnosticsInProgress) {
		IOCPURunPlatformPanicActions(type, details);
	}

skip_to_haltRestart:
	if (gIOPlatform) {
		// note that this will not necessarily halt or restart the system...
		// Implementors of this function will check the type and take action accordingly
		return gIOPlatform->haltRestart(type);
	} else {
		return -1;
	}
}

int
PEHaltRestart(unsigned int type)
{
	return PEHaltRestartInternal(type, 0);
}

UInt32
PESavePanicInfo(UInt8 *buffer, UInt32 length)
{
	if (gIOPlatform != NULL) {
		return (UInt32) gIOPlatform->savePanicInfo(buffer, length);
	} else {
		return 0;
	}
}

void
PESavePanicInfoAction(void *buffer, UInt32 offset, UInt32 length)
{
	IOCPURunPlatformPanicSyncAction(buffer, offset, length);
	return;
}


/*
 * Depending on the platform, the /options node may not be created
 * until after IOKit matching has started, by an externally-supplied
 * platform expert subclass.  Therefore, we must check for its presence
 * here and update gIOOptionsEntry for the platform code as necessary.
 */
inline static int
init_gIOOptionsEntry(void)
{
	IORegistryEntry *entry;
	void *nvram_entry;
	volatile void **options;
	int ret = -1;

	if (gIOOptionsEntry) {
		return 0;
	}

	entry = IORegistryEntry::fromPath( "/options", gIODTPlane );
	if (!entry) {
		return -1;
	}

	nvram_entry = (void *) OSDynamicCast(IODTNVRAM, entry);
	if (!nvram_entry) {
		goto release;
	}

	options = (volatile void **) &gIOOptionsEntry;
	if (!OSCompareAndSwapPtr(NULL, nvram_entry, options)) {
		ret = 0;
		goto release;
	}

	return 0;

release:
	entry->release();
	return ret;
}

/* pass in a NULL value if you just want to figure out the len */
boolean_t
PEReadNVRAMProperty(const char *symbol, void *value,
    unsigned int *len)
{
	OSObject  *obj;
	OSData *data;
	unsigned int vlen;

	if (!symbol || !len) {
		goto err;
	}

	if (init_gIOOptionsEntry() < 0) {
		goto err;
	}

	vlen = *len;
	*len = 0;

	obj = gIOOptionsEntry->getProperty(symbol);
	if (!obj) {
		goto err;
	}

	/* convert to data */
	data = OSDynamicCast(OSData, obj);
	if (!data) {
		goto err;
	}

	*len  = data->getLength();
	vlen  = min(vlen, *len);
	if (value && vlen) {
		memcpy((void *) value, data->getBytesNoCopy(), vlen);
	}

	return TRUE;

err:
	return FALSE;
}

boolean_t
PEReadNVRAMBooleanProperty(const char *symbol, boolean_t *value)
{
	OSObject  *obj;
	OSBoolean *data;

	if (!symbol || !value) {
		goto err;
	}

	if (init_gIOOptionsEntry() < 0) {
		goto err;
	}

	obj = gIOOptionsEntry->getProperty(symbol);
	if (!obj) {
		return TRUE;
	}

	/* convert to bool */
	data = OSDynamicCast(OSBoolean, obj);
	if (!data) {
		goto err;
	}

	*value = data->isTrue() ? TRUE : FALSE;

	return TRUE;

err:
	return FALSE;
}

boolean_t
PEWriteNVRAMBooleanProperty(const char *symbol, boolean_t value)
{
	const OSSymbol *sym = NULL;
	OSBoolean *data = NULL;
	bool ret = false;

	if (symbol == NULL) {
		goto exit;
	}

	if (init_gIOOptionsEntry() < 0) {
		goto exit;
	}

	if ((sym = OSSymbol::withCStringNoCopy(symbol)) == NULL) {
		goto exit;
	}

	data  = value ? kOSBooleanTrue : kOSBooleanFalse;
	ret = gIOOptionsEntry->setProperty(sym, data);

	sym->release();

	/* success, force the NVRAM to flush writes */
	if (ret == true) {
		gIOOptionsEntry->sync();
	}

exit:
	return ret;
}

static boolean_t
PEWriteNVRAMPropertyInternal(const char *symbol, boolean_t copySymbol, const void *value,
    const unsigned int len)
{
	const OSSymbol *sym;
	OSData *data;
	bool ret = false;

	if (!symbol || !value || !len) {
		goto err;
	}

	if (init_gIOOptionsEntry() < 0) {
		goto err;
	}

	if (copySymbol == TRUE) {
		sym = OSSymbol::withCString(symbol);
	} else {
		sym = OSSymbol::withCStringNoCopy(symbol);
	}

	if (!sym) {
		goto err;
	}

	data = OSData::withBytes((void *) value, len);
	if (!data) {
		goto sym_done;
	}

	ret = gIOOptionsEntry->setProperty(sym, data);
	data->release();

sym_done:
	sym->release();

	if (ret == true) {
		gIOOptionsEntry->sync();
		return TRUE;
	}

err:
	return FALSE;
}

boolean_t
PEWriteNVRAMProperty(const char *symbol, const void *value,
    const unsigned int len)
{
	return PEWriteNVRAMPropertyInternal(symbol, FALSE, value, len);
}

boolean_t
PEWriteNVRAMPropertyWithCopy(const char *symbol, const void *value,
    const unsigned int len)
{
	return PEWriteNVRAMPropertyInternal(symbol, TRUE, value, len);
}

boolean_t
PERemoveNVRAMProperty(const char *symbol)
{
	const OSSymbol *sym;

	if (!symbol) {
		goto err;
	}

	if (init_gIOOptionsEntry() < 0) {
		goto err;
	}

	sym = OSSymbol::withCStringNoCopy(symbol);
	if (!sym) {
		goto err;
	}

	gIOOptionsEntry->removeProperty(sym);

	sym->release();

	gIOOptionsEntry->sync();
	return TRUE;

err:
	return FALSE;
}

boolean_t
PESyncNVRAM(void)
{
	if (gIOOptionsEntry != nullptr) {
		gIOOptionsEntry->sync();
	}

	return TRUE;
}

long
PEGetGMTTimeOfDay(void)
{
	clock_sec_t     secs;
	clock_usec_t    usecs;

	PEGetUTCTimeOfDay(&secs, &usecs);
	return secs;
}

void
PESetGMTTimeOfDay(long secs)
{
	PESetUTCTimeOfDay(secs, 0);
}

void
PEGetUTCTimeOfDay(clock_sec_t * secs, clock_usec_t * usecs)
{
	clock_nsec_t    nsecs = 0;

	*secs = 0;
	if (gIOPlatform) {
		gIOPlatform->getUTCTimeOfDay(secs, &nsecs);
	}

	assert(nsecs < NSEC_PER_SEC);
	*usecs = nsecs / NSEC_PER_USEC;
}

void
PESetUTCTimeOfDay(clock_sec_t secs, clock_usec_t usecs)
{
	assert(usecs < USEC_PER_SEC);
	if (gIOPlatform) {
		gIOPlatform->setUTCTimeOfDay(secs, usecs * NSEC_PER_USEC);
	}
}

coprocessor_type_t
PEGetCoprocessorVersion( void )
{
	coprocessor_type_t coprocessor_version = kCoprocessorVersionNone;
#if defined(__x86_64__)
	IORegistryEntry     *platform_entry = NULL;
	OSData              *coprocessor_version_obj = NULL;

	platform_entry = IORegistryEntry::fromPath(kIODeviceTreePlane ":/efi/platform");
	if (platform_entry != NULL) {
		coprocessor_version_obj = OSDynamicCast(OSData, platform_entry->getProperty("apple-coprocessor-version"));
		if ((coprocessor_version_obj != NULL) && (coprocessor_version_obj->getLength() <= sizeof(uint64_t))) {
			memcpy(&coprocessor_version, coprocessor_version_obj->getBytesNoCopy(), coprocessor_version_obj->getLength());
		}
		platform_entry->release();
	}
#endif
	return coprocessor_version;
}
} /* extern "C" */

bool gIOPlatformUUIDAndSerialDone = false;

void
IOPlatformExpert::publishPlatformUUIDAndSerial( void )
{
	if (!gIOPlatformUUIDAndSerialDone) {
		// Parse the serial-number data and publish a user-readable string
		if (NULL == getProvider()->getProperty(kIOPlatformSerialNumberKey)) {
			OSData* mydata = (OSData*) (getProvider()->getProperty("serial-number"));
			if (mydata != NULL) {
				OSString *serNoString = createSystemSerialNumberString(mydata);
				if (serNoString != NULL) {
					getProvider()->setProperty(kIOPlatformSerialNumberKey, serNoString);
					serNoString->release();
				}
			}
		}
		IOPlatformExpertDevice *provider = OSDynamicCast(IOPlatformExpertDevice, getProvider());
		assert(provider != NULL);
		provider->generatePlatformUUID();
	}

	if (gIOPlatformUUIDAndSerialDone) {
		publishResource(kIOPlatformUUIDKey, getProvider()->getProperty(kIOPlatformUUIDKey));
	}
}

void
IOPlatformExpert::publishNVRAM( void )
{
	if (init_gIOOptionsEntry() < 0) {
		IOPlatformExpertDevice *provider = OSDynamicCast(IOPlatformExpertDevice, getProvider());
		assert(provider != NULL);
		provider->createNVRAM();
	}
	if (gIOOptionsEntry != NULL) {
		gIOOptionsEntry->registerService();
	}
}

void
IOPlatformExpert::registerNVRAMController(IONVRAMController * caller)
{
#if defined(__x86_64__)
	OSData *          data;
	IORegistryEntry * entry;

	/*
	 * If we have panic debugging enabled WITHOUT behavior to reboot after any crash (DB_REBOOT_ALWAYS)
	 * and we are on a co-processor system that has the panic SoC watchdog enabled, disable
	 * cross panics so that the co-processor doesn't cause the system
	 * to reset when we enter the debugger or hit a panic on the x86 side.
	 */
	if (panicDebugging && !(debug_boot_arg & DB_REBOOT_ALWAYS)) {
		entry = IORegistryEntry::fromPath( "/options", gIODTPlane );
		if (entry) {
			data = OSDynamicCast( OSData, entry->getProperty( APPLE_VENDOR_VARIABLE_GUID":BridgeOSPanicWatchdogEnabled" ));
			if (data && (data->getLength() == sizeof(UInt8))) {
				UInt8 *panicWatchdogEnabled = (UInt8 *) data->getBytesNoCopy();
				UInt32 debug_flags = 0;
				if (*panicWatchdogEnabled || (PE_i_can_has_debugger(&debug_flags) &&
				    (debug_flags & DB_DISABLE_CROSS_PANIC))) {
					coprocessor_cross_panic_enabled = FALSE;
				}
			}
			entry->release();
		}
	}

#if (DEVELOPMENT || DEBUG)
	entry = IORegistryEntry::fromPath( "/options", gIODTPlane );
	if (entry) {
		data = OSDynamicCast( OSData, entry->getProperty(nvram_osenvironment));
		if (data) {
			sysctl_set_osenvironment(data->getLength(), data->getBytesNoCopy());
			entry->removeProperty(nvram_osenvironment);
			IODTNVRAM * nvramOptionsEntry = OSDynamicCast(IODTNVRAM, entry);
			if (nvramOptionsEntry) {
				nvramOptionsEntry->sync();
			}
		}
		entry->release();
	}
	sysctl_unblock_osenvironment();
#endif
	/* on intel the UUID must be published after nvram is available */
	publishPlatformUUIDAndSerial();

#endif /* defined(__x86_64__) */

	publishResource("IONVRAM");
}

IOReturn
IOPlatformExpert::callPlatformFunction(const OSSymbol *functionName,
    bool waitForFunction,
    void *param1, void *param2,
    void *param3, void *param4)
{
	IOService *service, *_resources;
	OSObject  *prop = NULL;
	IOReturn   ret;

	if (functionName == gIOPlatformQuiesceActionKey ||
	    functionName == gIOPlatformActiveActionKey ||
	    functionName == gIOPlatformPanicActionKey) {
		/*
		 * Services which register for IOPlatformQuiesceAction / IOPlatformActiveAction / IOPlatformPanicAction
		 * must consume that event themselves, without passing it up to super/IOPlatformExpert.
		 */
		if (gEnforcePlatformActionSafety) {
			panic("Class %s passed the %s action to IOPlatformExpert",
			    getMetaClass()->getClassName(), functionName->getCStringNoCopy());
		}
	}

	if (waitForFunction) {
		_resources = waitForService(resourceMatching(functionName));
	} else {
		_resources = getResourceService();
	}
	if (_resources == NULL) {
		return kIOReturnUnsupported;
	}

	prop = _resources->copyProperty(functionName);
	service = OSDynamicCast(IOService, prop);
	if (service == NULL) {
		ret = kIOReturnUnsupported;
		goto finish;
	}

	ret = service->callPlatformFunction(functionName, waitForFunction,
	    param1, param2, param3, param4);

finish:
	OSSafeReleaseNULL(prop);
	return ret;
}

IOByteCount
IOPlatformExpert::savePanicInfo(UInt8 *buffer, IOByteCount length)
{
	return 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOPlatformExpert

OSDefineMetaClassAndAbstractStructors( IODTPlatformExpert, IOPlatformExpert )

OSMetaClassDefineReservedUnused(IODTPlatformExpert, 0);
OSMetaClassDefineReservedUnused(IODTPlatformExpert, 1);
OSMetaClassDefineReservedUnused(IODTPlatformExpert, 2);
OSMetaClassDefineReservedUnused(IODTPlatformExpert, 3);
OSMetaClassDefineReservedUnused(IODTPlatformExpert, 4);
OSMetaClassDefineReservedUnused(IODTPlatformExpert, 5);
OSMetaClassDefineReservedUnused(IODTPlatformExpert, 6);
OSMetaClassDefineReservedUnused(IODTPlatformExpert, 7);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService *
IODTPlatformExpert::probe( IOService * provider,
    SInt32 * score )
{
	if (!super::probe( provider, score)) {
		return NULL;
	}

	// check machine types
	if (!provider->compareNames( getProperty( gIONameMatchKey ))) {
		return NULL;
	}

	return this;
}

bool
IODTPlatformExpert::configure( IOService * provider )
{
	if (!super::configure( provider)) {
		return false;
	}

	processTopLevel( provider );

	return true;
}

IOService *
IODTPlatformExpert::createNub( IORegistryEntry * from )
{
	IOService *         nub;

	nub = new IOPlatformDevice;
	if (nub) {
		if (!nub->init( from, gIODTPlane )) {
			nub->free();
			nub = NULL;
		}
	}
	return nub;
}

bool
IODTPlatformExpert::createNubs( IOService * parent, OSIterator * iter )
{
	IORegistryEntry *   next;
	IOService *         nub = NULL;
	bool                ok = true;

	if (iter) {
		while ((next = (IORegistryEntry *) iter->getNextObject())) {
			OSSafeReleaseNULL(nub);

			if (NULL == (nub = createNub( next ))) {
				continue;
			}

			nub->attach( parent );
#if !defined(__x86_64__)
			OSData *tmpData = (OSData *)next->getProperty("device_type");
			if (tmpData == NULL) {
				nub->registerService();
				continue;
			}

			char *device_type = (char *)tmpData->getBytesNoCopy();
			if (strcmp(device_type, "cpu") != 0) {
				nub->registerService();
				continue;
			}

			tmpData = (OSData *)next->getProperty("reg");
			assert(tmpData != NULL);
			assert(tmpData->getLength() >= sizeof(UInt32));

			uint32_t phys_id = *(UInt32 *)tmpData->getBytesNoCopy();
			int logical_cpu_id = ml_get_cpu_number(phys_id);
			int logical_cluster_id = ml_get_cluster_number(phys_id);

			/*
			 * If the following condition triggers, it means that a CPU that was present in the DT
			 * was ignored by XNU at topology parsing time. This can happen currently when using the
			 * cpus=N boot-arg; for example, cpus=1 will cause XNU to parse and enable a single CPU.
			 *
			 * Note that this condition will not trigger for harvested cores because these do not show up
			 * in the DT/IORegistry in the first place.
			 */
			if (logical_cpu_id < 0) {
				nub->registerService();
				continue;
			}

			__assert_only bool logical_id_added_to_ioreg = nub->setProperty("logical-cpu-id", logical_cpu_id, 32U);
			assert(logical_id_added_to_ioreg == true);
			logical_id_added_to_ioreg = nub->setProperty("logical-cluster-id", logical_cluster_id, 32U);
			assert(logical_id_added_to_ioreg == true);
#endif
			nub->registerService();
		}
		OSSafeReleaseNULL(nub);
		iter->release();
	}

	return ok;
}

void
IODTPlatformExpert::processTopLevel( IORegistryEntry * rootEntry )
{
	OSIterator *        kids;
	IORegistryEntry *   next;
	IORegistryEntry *   cpus;

	// infanticide
	kids = IODTFindMatchingEntries( rootEntry, 0, deleteList());
	if (kids) {
		while ((next = (IORegistryEntry *)kids->getNextObject())) {
			next->detachAll( gIODTPlane);
		}
		kids->release();
	}

	publishNVRAM();
	assert(gIOOptionsEntry != NULL); // subclasses that do their own NVRAM initialization shouldn't be calling this
	dtNVRAM = gIOOptionsEntry;

	// Publish the cpus.
	cpus = rootEntry->childFromPath( "cpus", gIODTPlane);
	if (cpus) {
		createNubs( this, IODTFindMatchingEntries( cpus, kIODTExclusive, NULL));
		cpus->release();
	}

	// publish top level, minus excludeList
	createNubs( this, IODTFindMatchingEntries( rootEntry, kIODTExclusive, excludeList()));
}

IOReturn
IODTPlatformExpert::getNubResources( IOService * nub )
{
	if (nub->getDeviceMemory()) {
		return kIOReturnSuccess;
	}

	IODTResolveAddressing( nub, "reg", NULL);

	return kIOReturnSuccess;
}

bool
IODTPlatformExpert::compareNubName( const IOService * nub,
    OSString * name, OSString ** matched ) const
{
	return IODTCompareNubName( nub, name, matched )
	       || super::compareNubName( nub, name, matched);
}


/*
 * Do not use this method directly, it returns inconsistent results
 * across architectures and is considered deprecated.
 *
 * Use getTargetName and getProductName respectively.  For example:
 *
 * targetName: J137AP
 * productName: iMacPro1,1
 *
 * targetName: D331pAP
 * productName: iPhone11,6
 */

bool
IODTPlatformExpert::getModelName( char * name, int maxLength )
{
	OSData *            prop;
	const char *        str;
	int                 len;
	char                c;
	bool                ok = false;

	maxLength--;

	prop = (OSData *) getProvider()->getProperty( gIODTCompatibleKey );
	if (prop) {
		str = (const char *) prop->getBytesNoCopy();

		if (0 == strncmp( str, "AAPL,", strlen( "AAPL," ))) {
			str += strlen( "AAPL," );
		}

		len = 0;
		while ((c = *str++)) {
			if ((c == '/') || (c == ' ')) {
				c = '-';
			}

			name[len++] = c;
			if (len >= maxLength) {
				break;
			}
		}

		name[len] = 0;
		ok = true;
	}
	return ok;
}

/*
 * Do not use this method directly, it returns inconsistent results
 * across architectures and is considered deprecated.
 *
 * Use getTargetName and getProductName respectively.  For example:
 *
 * targetName: J137AP
 * productName: iMacPro1,1
 *
 * targetName: D331pAP
 * productName: iPhone11,6
 */

bool
IODTPlatformExpert::getMachineName( char * name, int maxLength )
{
	OSData *            prop;
	bool                ok = false;

	maxLength--;
	prop = (OSData *) getProvider()->getProperty( gIODTModelKey );
	ok = (NULL != prop);

	if (ok) {
		strlcpy( name, (const char *) prop->getBytesNoCopy(), maxLength );
	}

	return ok;
}

/* Examples: J137AP, D331pAP... */

bool
IODTPlatformExpert::getTargetName( char * name, int maxLength )
{
#if __x86_64__
	OSData *            prop;

	const OSSymbol *        key = gIODTBridgeModelKey;

	maxLength--;
	prop = (OSData *) getProvider()->getProperty( key );

	if (prop == NULL) {
		// This happens if there is no bridge.
		char const * const  unknown = "";

		strlcpy( name, unknown, maxLength );
	} else {
		strlcpy( name, (const char *)prop->getBytesNoCopy(), maxLength );
	}

	return true;
#else
	return getModelName( name, maxLength );
#endif
}

/* Examples: iMacPro1,1, iPhone11,6... */

bool
IODTPlatformExpert::getProductName( char * name, int maxLength )
{
#if __x86_64__
	return getModelName( name, maxLength );
#else
	return getMachineName( name, maxLength );
#endif
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IODTPlatformExpert::registerNVRAMController( IONVRAMController * nvram )
{
	if (dtNVRAM) {
		dtNVRAM->registerNVRAMController(nvram);
	}

	super::registerNVRAMController(nvram);
}

int
IODTPlatformExpert::haltRestart(unsigned int type)
{
	return super::haltRestart(type);
}

IOReturn
IODTPlatformExpert::readXPRAM(IOByteCount offset, UInt8 * buffer,
    IOByteCount length)
{
	if (dtNVRAM) {
		return dtNVRAM->readXPRAM(offset, buffer, length);
	} else {
		return kIOReturnNotReady;
	}
}

IOReturn
IODTPlatformExpert::writeXPRAM(IOByteCount offset, UInt8 * buffer,
    IOByteCount length)
{
	if (dtNVRAM) {
		return dtNVRAM->writeXPRAM(offset, buffer, length);
	} else {
		return kIOReturnNotReady;
	}
}

IOReturn
IODTPlatformExpert::readNVRAMProperty(
	IORegistryEntry * entry,
	const OSSymbol ** name, OSData ** value )
{
	if (dtNVRAM) {
		return dtNVRAM->readNVRAMProperty(entry, name, value);
	} else {
		return kIOReturnNotReady;
	}
}

IOReturn
IODTPlatformExpert::readNVRAMProperty(
	IORegistryEntry * entry,
	OSSharedPtr<const OSSymbol>& name, OSSharedPtr<OSData>& value )
{
	const OSSymbol* nameRaw = NULL;
	OSData* valueRaw = NULL;

	IOReturn result = readNVRAMProperty(entry, &nameRaw, &valueRaw);

	name.reset(nameRaw, OSNoRetain);
	value.reset(valueRaw, OSNoRetain);

	return result;
}

IOReturn
IODTPlatformExpert::writeNVRAMProperty(
	IORegistryEntry * entry,
	const OSSymbol * name, OSData * value )
{
	if (dtNVRAM) {
		return dtNVRAM->writeNVRAMProperty(entry, name, value);
	} else {
		return kIOReturnNotReady;
	}
}

OSDictionary *
IODTPlatformExpert::getNVRAMPartitions(void)
{
	if (dtNVRAM) {
		return dtNVRAM->getNVRAMPartitions();
	} else {
		return NULL;
	}
}

IOReturn
IODTPlatformExpert::readNVRAMPartition(const OSSymbol * partitionID,
    IOByteCount offset, UInt8 * buffer,
    IOByteCount length)
{
	if (dtNVRAM) {
		return dtNVRAM->readNVRAMPartition(partitionID, offset,
		           buffer, length);
	} else {
		return kIOReturnNotReady;
	}
}

IOReturn
IODTPlatformExpert::writeNVRAMPartition(const OSSymbol * partitionID,
    IOByteCount offset, UInt8 * buffer,
    IOByteCount length)
{
	if (dtNVRAM) {
		return dtNVRAM->writeNVRAMPartition(partitionID, offset,
		           buffer, length);
	} else {
		return kIOReturnNotReady;
	}
}

IOByteCount
IODTPlatformExpert::savePanicInfo(UInt8 *buffer, IOByteCount length)
{
	IOByteCount lengthSaved = 0;

	if (dtNVRAM) {
		lengthSaved = dtNVRAM->savePanicInfo(buffer, length);
	}

	if (lengthSaved == 0) {
		lengthSaved = super::savePanicInfo(buffer, length);
	}

	return lengthSaved;
}

OSString*
IODTPlatformExpert::createSystemSerialNumberString(OSData* myProperty)
{
	UInt8* serialNumber;
	unsigned int serialNumberSize;
	unsigned short pos = 0;
	char* temp;
	char SerialNo[30];

	if (myProperty != NULL) {
		serialNumberSize = myProperty->getLength();
		serialNumber = (UInt8*)(myProperty->getBytesNoCopy());
		temp = (char*)serialNumber;
		if (serialNumberSize > 0) {
			// check to see if this is a CTO serial number...
			while (pos < serialNumberSize && temp[pos] != '-') {
				pos++;
			}

			if (pos < serialNumberSize) { // there was a hyphen, so it's a CTO serial number
				memcpy(SerialNo, serialNumber + 12, 8);
				memcpy(&SerialNo[8], serialNumber, 3);
				SerialNo[11] = '-';
				memcpy(&SerialNo[12], serialNumber + 3, 8);
				SerialNo[20] = 0;
			} else { // just a normal serial number
				memcpy(SerialNo, serialNumber + 13, 8);
				memcpy(&SerialNo[8], serialNumber, 3);
				SerialNo[11] = 0;
			}
			return OSString::withCString(SerialNo);
		}
	}
	return NULL;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndStructors(IOPlatformExpertDevice, IOService)

OSMetaClassDefineReservedUnused(IOPlatformExpertDevice, 0);
OSMetaClassDefineReservedUnused(IOPlatformExpertDevice, 1);
OSMetaClassDefineReservedUnused(IOPlatformExpertDevice, 2);
OSMetaClassDefineReservedUnused(IOPlatformExpertDevice, 3);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool
IOPlatformExpertDevice::compareName( OSString * name,
    OSString ** matched ) const
{
	return IODTCompareNubName( this, name, matched );
}

bool
IOPlatformExpertDevice::init(void *dtRoot)
{
	IORegistryEntry *   dt = NULL;
	bool                ok;

	if ((dtRoot != NULL) && (dt = IODeviceTreeAlloc(dtRoot))) {
		ok = super::init( dt, gIODTPlane );
	} else {
		ok = super::init();
	}

	if (!ok) {
		return false;
	}

	return true;
}

bool
IOPlatformExpertDevice::startIOServiceMatching(void)
{
	workLoop = IOWorkLoop::workLoop();
	if (!workLoop) {
		return false;
	}

	registerService();

	return true;
}

IOWorkLoop *
IOPlatformExpertDevice::getWorkLoop() const
{
	return workLoop;
}

IOReturn
IOPlatformExpertDevice::setProperties( OSObject * properties )
{
	return kIOReturnUnsupported;
}

IOReturn
IOPlatformExpertDevice::newUserClient( task_t owningTask, void * securityID,
    UInt32 type, OSDictionary * properties,
    IOUserClient ** handler )
{
	IOReturn            err = kIOReturnSuccess;
	IOUserClient *      newConnect = NULL;
	IOUserClient *      theConnect = NULL;

	switch (type) {
	case kIOKitDiagnosticsClientType:
		newConnect = IOKitDiagnosticsClient::withTask(owningTask);
		if (!newConnect) {
			err = kIOReturnNotPermitted;
		}
		break;
	case kIOKitUserServerClientType:
		newConnect = IOUserServer::withTask(owningTask);
		if (!newConnect) {
			err = kIOReturnNotPermitted;
		}
		break;
	default:
		err = kIOReturnBadArgument;
	}

	if (newConnect) {
		if ((false == newConnect->attach(this))
		    || (false == newConnect->start(this))) {
			newConnect->detach( this );
			newConnect->release();
			err = kIOReturnNotPermitted;
		} else {
			theConnect = newConnect;
		}
	}

	*handler = theConnect;
	return err;
}

void
IOPlatformExpertDevice::free()
{
	if (workLoop) {
		workLoop->release();
	}
}

void
IOPlatformExpertDevice::configureDefaults( void )
{
	createNVRAM();
	// Parse the serial-number data and publish a user-readable string
	OSData* mydata = (OSData*) (getProperty("serial-number"));
	if (mydata != NULL) {
		OSString *serNoString = OSString::withCString((const char *)mydata->getBytesNoCopy());
		if (serNoString != NULL) {
			setProperty(kIOPlatformSerialNumberKey, serNoString);
			serNoString->release();
		}
	}
	generatePlatformUUID();
}

void
IOPlatformExpertDevice::createNVRAM( void )
{
	/*
	 * Publish an IODTNVRAM class on /options, if present.
	 * DT-based platforms may need NVRAM access prior to the start
	 * of IOKit matching, to support security-related operations
	 * that must happen before machine_lockdown().
	 */
	IORegistryEntry *options = IORegistryEntry::fromPath("/options", gIODTPlane);
	if (options == NULL) {
		return; // /options may not be present
	}

	assert(gIOOptionsEntry == NULL);
	gIOOptionsEntry = new IODTNVRAM;

	assert(gIOOptionsEntry != NULL);

	gIOOptionsEntry->init(options, gIODTPlane);
	gIOOptionsEntry->attach(this);
	gIOOptionsEntry->start(this);
	options->release();
}

void
IOPlatformExpertDevice::generatePlatformUUID( void )
{
	IORegistryEntry * entry;
	OSString *        string = NULL;
	uuid_string_t     uuid;

#if !defined(__x86_64__)
	entry = IORegistryEntry::fromPath( "/chosen", gIODTPlane );
	if (entry) {
		OSData * data1;

		data1 = OSDynamicCast( OSData, entry->getProperty( "unique-chip-id" ));
		if (data1 && data1->getLength() == 8) {
			OSData * data2;

			data2 = OSDynamicCast( OSData, entry->getProperty( "chip-id" ));
			if (data2 && data2->getLength() == 4) {
				SHA1_CTX     context;
				uint8_t      digest[SHA_DIGEST_LENGTH];
				const uuid_t space = { 0xA6, 0xDD, 0x4C, 0xCB, 0xB5, 0xE8, 0x4A, 0xF5, 0xAC, 0xDD, 0xB6, 0xDC, 0x6A, 0x05, 0x42, 0xB8 };

				SHA1Init( &context );
				SHA1Update( &context, space, sizeof(space));
				SHA1Update( &context, data1->getBytesNoCopy(), data1->getLength());
				SHA1Update( &context, data2->getBytesNoCopy(), data2->getLength());
				SHA1Final( digest, &context );

				digest[6] = (digest[6] & 0x0F) | 0x50;
				digest[8] = (digest[8] & 0x3F) | 0x80;

				uuid_unparse( digest, uuid );
				string = OSString::withCString( uuid );
			}
		}

		entry->release();
	}
#else /* !defined(__x86_64__) */
	OSData * data;

	entry = IORegistryEntry::fromPath( "/efi/platform", gIODTPlane );
	if (entry) {
		data = OSDynamicCast( OSData, entry->getProperty( "system-id" ));
		if (data && data->getLength() == 16) {
			SHA1_CTX     context;
			uint8_t      digest[SHA_DIGEST_LENGTH];
			const uuid_t space = { 0x2A, 0x06, 0x19, 0x90, 0xD3, 0x8D, 0x44, 0x40, 0xA1, 0x39, 0xC4, 0x97, 0x70, 0x37, 0x65, 0xAC };

			SHA1Init( &context );
			SHA1Update( &context, space, sizeof(space));
			SHA1Update( &context, data->getBytesNoCopy(), data->getLength());
			SHA1Final( digest, &context );

			digest[6] = (digest[6] & 0x0F) | 0x50;
			digest[8] = (digest[8] & 0x3F) | 0x80;

			uuid_unparse( digest, uuid );
			string = OSString::withCString( uuid );
		}

		entry->release();
	}
	if (!string) {
		/* vmware still runs this path */
		entry = IORegistryEntry::fromPath( "/options", gIODTPlane );
		if (entry) {
			data = OSDynamicCast( OSData, entry->getProperty( "platform-uuid" ));
			if (data && data->getLength() == sizeof(uuid_t)) {
				uuid_unparse((uint8_t *) data->getBytesNoCopy(), uuid );
				string = OSString::withCString( uuid );
			}
			entry->release();
		}
	}
#endif /* defined(__x86_64__) */

	if (string) {
		setProperty( kIOPlatformUUIDKey, string );
		gIOPlatformUUIDAndSerialDone = true;

		string->release();
	}
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndStructors(IOPlatformDevice, IOService)

OSMetaClassDefineReservedUnused(IOPlatformDevice, 0);
OSMetaClassDefineReservedUnused(IOPlatformDevice, 1);
OSMetaClassDefineReservedUnused(IOPlatformDevice, 2);
OSMetaClassDefineReservedUnused(IOPlatformDevice, 3);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool
IOPlatformDevice::compareName( OSString * name,
    OSString ** matched ) const
{
	return ((IOPlatformExpert *)getProvider())->
	       compareNubName( this, name, matched );
}

IOService *
IOPlatformDevice::matchLocation( IOService * /* client */ )
{
	return this;
}

IOReturn
IOPlatformDevice::getResources( void )
{
	return ((IOPlatformExpert *)getProvider())->getNubResources( this );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*********************************************************************
* IOPanicPlatform class
*
* If no legitimate IOPlatformDevice matches, this one does and panics
* the kernel with a suitable message.
*********************************************************************/

class IOPanicPlatform : IOPlatformExpert {
	OSDeclareDefaultStructors(IOPanicPlatform);

public:
	bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
};


OSDefineMetaClassAndStructors(IOPanicPlatform, IOPlatformExpert);


bool
IOPanicPlatform::start(IOService * provider)
{
	const char * platform_name = "(unknown platform name)";

	if (provider) {
		platform_name = provider->getName();
	}

	panic("Unable to find driver for this platform: \"%s\".",
	    platform_name);

	return false;
}
