export MakeInc_cmd=${SRCROOT}/makedefs/MakeInc.cmd
export MakeInc_def=${SRCROOT}/makedefs/MakeInc.def
export MakeInc_rule=${SRCROOT}/makedefs/MakeInc.rule
export MakeInc_dir=${SRCROOT}/makedefs/MakeInc.dir

include $(MakeInc_cmd)
include $(MakeInc_def)

#
# Keep INSTALLHDRS_SKIP_HOST set to YES for exclave builds,
# otherwise firehose headers will install into exclave SDKs.
# 
ifeq ($(filter 1,$(EXCLAVECORE) $(EXCLAVEKIT)),)
INSTALLHDRS_SKIP_HOST = NO
endif

KERNELFILES =

DATAFILES =

PRIVATE_KERNELFILES =

EXPORTFILES = \
	firehose_types_private.h \
	ioctl_private.h \
	tracepoint_private.h \
	chunk_private.h

PRIVATE_DATAFILES = ${EXPORTFILES} \
	private.h

PRIVATE_MODULEMAPFILES = \
	module.modulemap

INSTALL_MI_DIR = firehose

INSTALL_MODULEMAP_MI_DIR = firehose

INSTALL_MI_LIST	= ${DATAFILES}

INSTALL_MI_LCL_LIST = ${PRIVATE_DATAFILES}

INSTALL_MODULEMAP_MI_LCL_LIST = ${PRIVATE_MODULEMAPFILES}

INSTALL_KF_MI_LIST = ${KERNELFILES}

INSTALL_KF_MI_LCL_LIST = ${KERNELFILES} ${PRIVATE_KERNELFILES}

EXPORT_MI_DIR = firehose

EXPORT_MI_LIST = ${KERNELFILES} ${PRIVATE_KERNELFILES} ${EXPORTFILES}

include $(MakeInc_rule)
include $(MakeInc_dir)
