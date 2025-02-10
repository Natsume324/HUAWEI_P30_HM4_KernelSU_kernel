#
# Makefile for the GalaxyCore touchscreen driver.
#
ccflags-y += -I$(srctree)/drivers/devkit/tpkit/
obj-$(CONFIG_GALAXYCORE_TS_KIT) +=  gcore_drv_main.o gcore_drv_kit_ops.o gcore_drv_fw_update.o gcore_drv_interface.o gcore_drv_mp_test.o
EXTRA_CFLAGS += -Idrivers/devkit/tpkit/
