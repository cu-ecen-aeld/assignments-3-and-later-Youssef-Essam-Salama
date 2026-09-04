# See example Makefile from scull project
# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DAESD_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
# call from kernel build system
obj-m	:= aesdchar.o
aesdchar-y := aesd-circular-buffer.o main.o
else

# Native default: headers for the running kernel. Yocto/cross builds override
# KERNELDIR / KERNELOUT / CC / LD / AR / KCFLAGS (see aesd-assignments_git.bb).
# Do not forward Make's built-in CC/LD/AR defaults — that forces gcc 11 as "cc"
# against a gcc-12-built host kernel. Native CC defaults to the exact compiler
# recorded in the kernel build (CONFIG_CC_VERSION_TEXT).
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
KERNELOUT ?= $(KERNELDIR)
PWD       := $(shell pwd)

# Only pass through vars set on the command line or in the environment.
override_from_cli_or_env = $(filter command line environment,$(origin $1))

KERNEL_CC := $(shell sed -n 's/^CONFIG_CC_VERSION_TEXT=\([^ ]*\).*/\1/p' \
	$(KERNELDIR)/include/config/auto.conf 2>/dev/null)

KBUILD_ARGS :=
ifneq ($(call override_from_cli_or_env,KERNELOUT),)
KBUILD_ARGS += O=$(KERNELOUT)
endif
ifeq ($(call override_from_cli_or_env,CC),)
ifneq ($(KERNEL_CC),)
KBUILD_ARGS += CC="$(KERNEL_CC)"
endif
else
KBUILD_ARGS += CC="$(CC)"
endif
ifneq ($(call override_from_cli_or_env,LD),)
KBUILD_ARGS += LD="$(LD)"
endif
ifneq ($(call override_from_cli_or_env,AR),)
KBUILD_ARGS += AR="$(AR)"
endif
ifneq ($(call override_from_cli_or_env,KCFLAGS),)
KBUILD_ARGS += KCFLAGS="$(KCFLAGS)"
endif

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(KBUILD_ARGS) modules

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

