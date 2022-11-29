#
# Disable CCACHE
CCACHE_DISABLE=1
LDFLAGS += -Wl,-Bstatic -static-libgcc
#LDFLAGS += -Wl,-Bstatic
#
CPSW_DIR=/afs/slac/g/lcls/package/cpsw/framework/R4.4.1/src

# may override CPSW_DIR / DEVICELIB_DIR from release.mak
# must set SRCDIR (is redefined by recursive make)
SRCDIR= .
-include $(SRCDIR)/release.mak

include $(CPSW_DIR)/defs.mak
INSTALL_DIR=../

#HEADERS = RamControl.hh TPGMini.hh TPG.hh AmcCarrier.hh
CXXFLAGS = -g -DFRAMEWORK_R3_4
HEADERS = BsaField.hh Processor.hh BsaDefs.hh AmcCarrierBase.hh AmcCarrier.hh AmcCarrierYaml.hh BsssYaml.hh
bsa_SRCS += RamControl.cc TPGMini.cc TPG.cc AmcCarrierBase.cc AmcCarrier.cc AmcCarrierYaml.cc BsaDefs.cc BsssYaml.cc
bsa_SRCS += Processor.cc

STATIC_LIBRARIES+=bsa

bsa_tst_SRCS = bsa_tst.cc
bsa_tst_LIBS = bsa $(CPSW_LIBS)
PROGRAMS    += bsa_tst

bsaapp_tst_SRCS = bsaapp_tst.cc
bsaapp_tst_LIBS = bsa $(CPSW_LIBS)
PROGRAMS    += bsaapp_tst

bsayaml_tst_SRCS = bsayaml_tst.cc
bsayaml_tst_LIBS = bsa $(CPSW_LIBS)
#PROGRAMS    += bsayaml_tst

cpsw_duo_SRCS = cpsw_duo.cc
cpsw_duo_LIBS = bsa $(CPSW_LIBS)
PROGRAMS    += cpsw_duo

cpu_tst_SRCS = cpu_tst.cc
cpu_tst_LIBS = bsa $(CPSW_LIBS)
PROGRAMS    += cpu_tst

bld_control_SRCS = bld_control.cc
bld_control_LIBS = $(CPSW_LIBS)
PROGRAMS    += bld_control

tpr_stream_SRCS = tpr_stream.cc
tpr_stream_LIBS = $(CPSW_LIBS)
PROGRAMS    += tpr_stream

PROGRAMS = bsaapp_tst

include $(CPSW_DIR)/rules.mak
