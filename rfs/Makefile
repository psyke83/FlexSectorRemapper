#
# Makefile for the linux rfs-filesystem
#

ifeq ($(CONFIG_GCOV_PROFILE),y)
ifeq ($(PATCHLEVEL),4)
include Makefile.gcov
else
include $(srctree)/fs/rfs/Makefile.gcov
endif
# In profile mode, we always turn on log replay test without power-off
EXTRA_CFLAGS	+= -D_RFS_INTERNAL_RECOVERY_TEST
endif

#
# RFS internal configurations
#

# trace writing block
#EXTRA_CFLAGS += -D_RFS_INTERNAL_STAT_BH

# support quota, it is experimental feature
#EXTRA_CFLAGS += -D_RFS_INTERNAL_QUOTA

# Flush user cache
#EXTRA_CFLAGS	+= -D_RFS_INTERNAL_FLUSH_CACHE

# check sanity of pool
#EXTRA_CFLAGS += -D_RFS_INTERNAL_SANITY_CHECK

# log debug
#EXTRA_CFLAGS += -D_RFS_INTERNAL_LOG_DEBUG

# read-only mount
EXTRA_CFLAGS += -DRFS_RDONLY_MOUNT

# display debug information when symlink invokes
#EXTRA_CFLAGS += -D_RFS_INTERNAL_SYMLINK_DEBUG

# dump log file when log file was crashed
EXTRA_CFLAGS += -D_RFS_INTERNAL_PRINT_ON_LOG_CRASH

# check sanity of log
#EXTRA_CFLAGS += -D_RFS_INTERNAL_SANITY_CHECK_MOUNT_LOG ## mount-time check

# runtime check sanity of log
#EXTRA_CFLAGS += -D_RFS_INTERNAL_SANITY_CHECK_LOG       ## run-time check

# remount read-only on error
EXTRA_CFLAGS += -DRFS_ERRORS_RDONLY

# panic on error
#EXTRA_CFLAGS += -DRFS_ERRORS_PANIC

# bugs on error
EXTRA_CFLAGS += -DRFS_ERRORS_BUG

# crash on error : break PBR if filesystem detects pool corruption
EXTRA_CFLAGS += -DRFS_ERRORS_CRASH_PBR

# notify on cluster allocation change
#EXTRA_CFLAGS += -DRFS_CLUSTER_CHANGE_NOTIFY

# xattr R2 debug xattr alloc
#EXTRA_CFLAGS += -DRFS_INTERNAL_R2_DEBUG_XATTR_ALLOC

# xattr R2 debug xattr set
#EXTRA_CFLAGS += -DRFS_INTERNAL_R2_DEBUG_XATTR_SET

# internal proc for hot/cold data mgnt 
#EXTRA_CFLAGS += -DRFS_INTERNAL_PROC_HOT_BOUNDARY

# Mount time optimization
EXTRA_CFLAGS += -D_RFS_INTERNAL_MOUNT_OPT

ifeq ($(PATCHLEVEL),4)
include Makefile.24
else
include $(srctree)/fs/rfs/Makefile.26
endif
