#
# Makefile for the Qlogic fibre channel drivers
#
# Note : at this point, these files are compiled on all systems.
# In the future, some of these should be built conditionally.
#

O_TARGET := qla.o
EXTRA_CFLAGS := -I../../scsi
obj-$(CONFIG_FC_QLA2200)	+= qla2200.o
obj-$(CONFIG_FC_QLA2300)	+= qla2300.o

include $(TOPDIR)/Rules.make
