CONFIG_MODULE_SIG=n

ifneq ($(KERNELRELEASE),)
	obj-m := TP2.o
else
	KERNEL_DIR ?= /lib/modules/4.19.66-v7+/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C ${KERNEL_DIR} M=$(PWD) modules
endif
