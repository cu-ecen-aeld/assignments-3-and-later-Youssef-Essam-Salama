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
# KERNELDIR and KERNELOUT (see aesd-assignments_git.bb).
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
KERNELOUT ?= $(KERNELDIR)
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) O=$(KERNELOUT) M=$(PWD) \
		$(if $(CC),CC="$(CC)",) \
		$(if $(LD),LD="$(LD)",) \
		$(if $(AR),AR="$(AR)",) \
		$(if $(KCFLAGS),KCFLAGS="$(KCFLAGS)",) \
		modules

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

