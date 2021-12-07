ifneq ($(KERNELRELEASE),)
src-m := qwx_ioe_driver.c
obj-m := $(patsubst %.c,%.o,$(src-m))
else
KDIR := $(HOME)/iso/linux/linux-stable
all:
	$(MAKE) -C $(KDIR) M=$$PWD
endif