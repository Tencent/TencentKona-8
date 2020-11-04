#
# Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#
#

# This makefile (trace.make) is included from the trace.make in the
# build directories.
#
# It knows how to build and run the tools to generate trace files.

include $(GAMMADIR)/make/solaris/makefiles/rules.make
include $(GAMMADIR)/make/altsrc.make

# #########################################################################

HAS_ALT_SRC:=$(shell if [ -d $(HS_ALT_SRC)/share/vm/trace ]; then \
  echo "true"; else echo "false";\
  fi)

TOPDIR      = $(shell echo `pwd`)
GENERATED   = $(TOPDIR)/../generated
JvmtiOutDir = $(GENERATED)/jvmtifiles
TraceOutDir   = $(GENERATED)/tracefiles

TraceAltSrcDir = $(HS_ALT_SRC)/share/vm/trace
TraceSrcDir = $(HS_COMMON_SRC)/share/vm/trace

# set VPATH so make knows where to look for source files
Src_Dirs_V += $(TraceSrcDir) $(TraceAltSrcDir)
VPATH += $(Src_Dirs_V:%=%:)

TraceGeneratedNames =     \
    traceEventClasses.hpp \
	traceEventIds.hpp     \
	traceTypes.hpp

ifeq ($(HAS_ALT_SRC), true)
TraceGeneratedNames +=  \
	traceRequestables.hpp \
    traceEventControl.hpp

ifneq ($(INCLUDE_TRACE), false)
  TraceGeneratedNames += traceProducer.cpp
endif

endif

TraceGeneratedFiles = $(TraceGeneratedNames:%=$(TraceOutDir)/%)

XSLT = $(REMOTE) $(RUN.JAVA) -classpath $(JvmtiOutDir) jvmtiGen

XML_DEPS =  $(TraceSrcDir)/trace.xml  $(TraceSrcDir)/tracetypes.xml \
	$(TraceSrcDir)/trace.dtd $(TraceSrcDir)/xinclude.mod
ifeq ($(HAS_ALT_SRC), true)
	XML_DEPS += $(TraceAltSrcDir)/traceevents.xml
endif

.PHONY: all clean cleanall

# #########################################################################

all: $(TraceGeneratedFiles)

GENERATE_CODE= \
  $(QUIETLY) echo Generating $@; \
  $(XSLT) -IN $(word 1,$^) -XSL $(word 2,$^) -OUT $@; \
  test -f $@

$(TraceOutDir)/traceEventIds.hpp: $(TraceSrcDir)/trace.xml $(TraceSrcDir)/traceEventIds.xsl $(XML_DEPS)
	$(GENERATE_CODE)

$(TraceOutDir)/traceTypes.hpp: $(TraceSrcDir)/trace.xml $(TraceSrcDir)/traceTypes.xsl $(XML_DEPS)
	$(GENERATE_CODE)

ifeq ($(HAS_ALT_SRC), false)

$(TraceOutDir)/traceEventClasses.hpp: $(TraceSrcDir)/trace.xml $(TraceSrcDir)/traceEventClasses.xsl $(XML_DEPS)
	$(GENERATE_CODE)

else

$(TraceOutDir)/traceEventClasses.hpp: $(TraceSrcDir)/trace.xml $(TraceAltSrcDir)/traceEventClasses.xsl $(XML_DEPS)
	$(GENERATE_CODE)

$(TraceOutDir)/traceProducer.cpp: $(TraceSrcDir)/trace.xml $(TraceAltSrcDir)/traceProducer.xsl $(XML_DEPS)
	$(GENERATE_CODE)

$(TraceOutDir)/traceRequestables.hpp: $(TraceSrcDir)/trace.xml $(TraceAltSrcDir)/traceRequestables.xsl $(XML_DEPS)
	$(GENERATE_CODE)

$(TraceOutDir)/traceEventControl.hpp: $(TraceSrcDir)/trace.xml $(TraceAltSrcDir)/traceEventControl.xsl $(XML_DEPS)
	$(GENERATE_CODE)

endif

# #########################################################################

clean cleanall:
	rm $(TraceGeneratedFiles)


