#
# Makefile to run jtreg and other tests
#

# Product builds and langtools builds
#
# A full product build (or "control" build) creates a complete JDK image.
# To test a product build, set TESTJAVA to the path for the image.
#
# A langtools build just builds the langtools components of a JDK.
# To test a langtools build, set TESTJAVA to the path for a recent JDK
# build, and set TESTBOOTCLASSPATH to the compiled langtools classes --
# for example build/classes or dist/lib/classes.jar.

# JPRT
# JPRT may invoke this Makefile directly, as part of a langtools build,
# or indirectly, via FOREST/test/Makefile, as part of a control build.


# Utilities used
AWK       = awk
CAT       = cat
CD        = cd
CHMOD     = chmod
CP        = cp
CUT       = cut
DIRNAME   = dirname
ECHO      = echo
EGREP     = egrep
EXPAND    = expand
FIND      = find
MKDIR     = mkdir
PWD       = pwd
SED       = sed
SORT      = sort
TEE       = tee
UNAME     = uname
UNIQ      = uniq
WC        = wc
ZIP       = zip

# Get OS name from uname (Cygwin inexplicably adds _NT-5.1)
UNAME_S := $(shell $(UNAME) -s | $(CUT) -f1 -d_)

# Commands to run on paths to make mixed paths for java on windows
ifeq ($(UNAME_S), CYGWIN)
  # Location of developer shared files
  SLASH_JAVA = J:
  GETMIXEDPATH = cygpath -m
else
  # Location of developer shared files
  SLASH_JAVA = /java

  GETMIXEDPATH=$(ECHO)
endif

# Get OS/ARCH specifics
OSNAME = $(shell uname -s)
ifeq ($(OSNAME), SunOS)
  SLASH_JAVA = /java
  PLATFORM = solaris
  ARCH = $(shell uname -p)
  ifeq ($(ARCH), i386)
    ARCH=i586
  endif
endif
ifeq ($(OSNAME), Linux)
  SLASH_JAVA = /java
  PLATFORM = linux
  ARCH = $(shell uname -m)
  ifeq ($(ARCH), i386)
    ARCH=i586
  endif
endif
ifeq ($(OSNAME), Darwin)
  PLATFORM = bsd
  ARCH = $(shell uname -m)
  ifeq ($(ARCH), i386)
    ARCH=i586
  endif
endif
ifeq ($(OSNAME), Windows_NT)
  # MKS
  PLATFORM=windows
endif
ifeq ($(PLATFORM),)
  PLATFORM = windows
  CYGPATH = | cygpath -m -s -f -
endif

ifeq ($(PLATFORM), windows)
  SLASH_JAVA = J:
  ifeq ($(word 1, $(PROCESSOR_IDENTIFIER)),ia64)
    ARCH=ia64
  else
    ifeq ($(word 1, $(PROCESSOR_IDENTIFIER)),AMD64)
      ARCH=x64
    else
      ifeq ($(word 1, $(PROCESSOR_IDENTIFIER)),EM64T)
        ARCH=x64
      else
        ARCH=i586
      endif
    endif
  endif
  EXE_SUFFIX=.exe
endif

# Root of this test area (important to use full paths in some places)
TEST_ROOT := $(shell pwd $(CYGPATH) )

# Default bundle of all test results (passed or not) (JPRT only)
ifdef JPRT_JOB_ID
  JPRT_CLEAN = clean
  JPRT_ARCHIVE_BUNDLE = $(TEST_ROOT)/JPRT_ARCHIVE_BUNDLE.zip
endif

ifeq ($(PLATFORM), windows)
  SLASH_JAVA = J:
else
  SLASH_JAVA = /java
endif

# Default JTREG to run
ifdef JPRT_JTREG_HOME
  JTREG_HOME = $(JPRT_JTREG_HOME)
else
  JTREG_HOME = $(SLASH_JAVA)/re/jtreg/4.1/promoted/latest/binaries/jtreg
endif
JTREG = $(JTREG_HOME)/bin/jtreg
JTDIFF = $(JTREG_HOME)/bin/jtdiff

# Default JCK to run
ifdef JPRT_JCK_HOME
  JCK_HOME = $(JPRT_JCK_HOME)
else
  JCK_HOME = $(SLASH_JAVA)/re/jck/8/promoted/latest/binaries
endif

# Default JDK for JTREG and JCK
#
# JT_JAVA is the version of java used to run jtreg/JCK. 
#
ifdef JPRT_JAVA_HOME
  JT_JAVA = $(JPRT_JAVA_HOME)
else
  JT_JAVA = $(SLASH_JAVA)/re/jdk/1.9.0/archive/fcs/binaries/$(PLATFORM)-$(ARCH)
endif

# Default JDK to test
ifdef JPRT_IMPORT_PRODUCT_HOME
  TESTJAVA = $(JPRT_IMPORT_PRODUCT_HOME)
else
  TESTJAVA = $(SLASH_JAVA)/re/jdk/1.9.0/promoted/latest/binaries/$(PLATFORM)-$(ARCH)
endif

# PRODUCT_HOME is a JPRT variable pointing to a directory containing the output from
# make/Makefile
# For langtools, this is a directory containing build and dist
# For a control build, this is build/$(PRODUCT)-$(ARCH)/XYZ-image
#	(i.e, j2sdk-image or jdk-module-image)
ifdef PRODUCT_HOME
  ifeq ($(shell [ -r $(PRODUCT_HOME)/dist/lib/classes.jar ]; echo $$?),0)
    TESTBOOTCLASSPATH = $(PRODUCT_HOME)/dist/lib/classes.jar
  endif
  ifeq ($(shell [ -r $(PRODUCT_HOME)/bin/javac$(EXE_SUFFIX) ]; echo $$?),0)
    TESTJAVA = $(PRODUCT_HOME)
  endif
endif

ifdef TESTBOOTCLASSPATH
  JTREG_OPTIONS += -Xbootclasspath/p:$(TESTBOOTCLASSPATH)
### In the following, -refvmoptions is an undocumented option
### The following does not work JCK 7 b30 2/6/2010. Awaiting b31.
  JCK_OPTIONS += \
	-vmoptions:-Xbootclasspath/p:$(TESTBOOTCLASSPATH) \
	-refvmoptions:-Xbootclasspath/p:$(TESTBOOTCLASSPATH)
endif

# Concurrency is the number of tests that can execute at once.
# On an otherwise empty machine, suggest setting to (#cpus + 2)
# If unset, the default is (#cpus)
### RFE: determine and use #cpus
ifdef CONCURRENCY
  JTREG_OPTIONS += -agentvm -concurrency:$(CONCURRENCY)
else
  JTREG_OPTIONS += -agentvm
endif

ifdef JCK_CONCURRENCY
  JCK_OPTIONS += -concurrency:$(JCK_CONCURRENCY)
endif

# JCK is executed using "Multi-JVM Group Mode", which is a hybrid
# of otherVM and sameVM modes. New JVMs are created and reused for
# a number of tests, then eventually discarded and a new one started.
# This amortizes the JVM startup time.  The "group size" defines
# how many tests are run in a JVM before it is replaced.
# If unset, the default is 100.
JCK_GROUP_SIZE = 1000
ifdef JCK_GROUP_SIZE
  JCK_COMPILER_OPTIONS += \
    -jtoptions:-Ejck.env.compiler.testCompile.groupMode.groupSize=$(JCK_GROUP_SIZE) \
    -jtoptions:-Ejck.env.compiler.compRefExecute.groupMode.groupSize=$(JCK_GROUP_SIZE)
### The following is not supported. Awaiting RFE 6924287
### 6924287: Jck4Jdk: Allow to configure test group size for group mode via simple command line option
###  JCK_RUNTIME_OPTIONS += \
###    -jtoptions:-Ejck.env.runtime.testCompile.groupMode.groupSize=$(JCK_GROUP_SIZE)
endif

# Timeouts -- by default, increase test timeouts when running on JPRT
ifdef JPRT_JOB_ID
  ifndef JTREG_TIMEOUT_FACTOR
    JTREG_TIMEOUT_FACTOR = 3
  endif
endif
ifdef JTREG_TIMEOUT_FACTOR
  JTREG_OPTIONS += -timeoutFactor:$(JTREG_TIMEOUT_FACTOR)
endif

# Default verbosity setting for jtreg
JTREG_VERBOSE = fail,error,summary

# Default verbosity setting for jck
JCK_VERBOSE = non-pass

# Assertions: some tests show failures when assertions are enabled.
# Since javac is typically loaded via the bootclassloader (either via TESTJAVA
# or TESTBOOTCLASSPATH), you may need -esa to enable assertions in javac.
JTREG_OPTIONS += $(ASSERTION_OPTIONS)
JCK_OPTIONS += $(ASSERTION_OPTIONS:%=-vmoptions:%)

# Include shared options
JCK_COMPILER_OPTIONS += $(JCK_OPTIONS)
JCK_RUNTIME_OPTIONS += $(JCK_OPTIONS)

# Exit codes:
# jtreg, jck:   0: OK, 1: tests failed, 2: tests error; 3+: SERIOUS
FATAL_JTREG_EXIT = 3
FATAL_JCK_EXIT = 3
# jtdiff: 0: OK, 1: differences found; 2+: SERIOUS
FATAL_JTDIFF_EXIT = 2
#
# Exit -- used for final "normal" exit from "make". Redefine to "true" to avoid
# having make exit with non-zero return code.
EXIT = exit
# Function to exit shell if exit code of preceding command is greater than or equal
# to a given level. Redefine function or preceding FATAL_*_EXIT codes as needed.
EXIT_IF_FATAL = status=$$?; if [ $$status -ge $(1) ]; then exit $$status ; fi

# The test directories to run
DEFAULT_TESTDIRS = .
TESTDIRS = $(DEFAULT_TESTDIRS)

# Root of all test results
TEST_OUTPUT_DIR = $(TEST_ROOT)/../build/$(PLATFORM)-$(ARCH)/test/langtools
ifdef ALT_OUTPUTDIR
  ABS_OUTPUTDIR := $(shell cd $(ALT_OUTPUTDIR) && pwd $(CYGPATH))
else
  ABS_OUTPUTDIR := $(shell mkdir -p $(TEST_OUTPUT_DIR); cd $(TEST_OUTPUT_DIR) && pwd $(CYGPATH))
endif
ABS_TEST_OUTPUT_DIR := $(ABS_OUTPUTDIR)/testoutput/$(UNIQUE_DIR)
# Subdirectories for different test runs
JTREG_OUTPUT_DIR = $(ABS_OUTPUTDIR)/jtreg
JCK_COMPILER_OUTPUT_DIR = $(ABS_OUTPUTDIR)/jck-compiler
JCK_RUNTIME_OUTPUT_DIR = $(ABS_OUTPUTDIR)/jck-runtime-Xcompile

# Problematic tests to be excluded
PROBLEM_LISTS=$(call MixedDirs,$(wildcard ProblemList.txt closed/ProblemList.txt))

# # Create exclude list for this platform and arch
ifdef NO_EXCLUDES
  JTREG_EXCLUSIONS =
else
  ifeq ($(JTREG_EXCLUSIONS),)
	JTREG_EXCLUSIONS = $(PROBLEM_LISTS:%=-exclude:%)
  endif
endif

# convert list of directories to dos paths
define MixedDirs
  $(foreach i,$1,$(shell $(GETMIXEDPATH) "${i}"))
endef

# Default make rule -- warning, may take a while
all: $(JPRT_CLEAN) jtreg-tests jck-compiler-tests jck-runtime-tests $(JPRT_ARCHIVE_BUNDLE) all-summary
	@echo "Testing completed successfully"

jtreg apt javac javadoc javah javap jdeps: $(JPRT_CLEAN) jtreg-tests $(JPRT_ARCHIVE_BUNDLE) jtreg-summary
	@echo "Testing completed successfully"

jck-compiler: $(JPRT_CLEAN) jck-compiler-tests $(JPRT_ARCHIVE_BUNDLE) jck-compiler-summary
	@echo "Testing completed successfully"

jck-runtime: $(JPRT_CLEAN) jck-runtime-tests $(JPRT_ARCHIVE_BUNDLE) jck-runtime-summary
	@echo "Testing completed successfully"

tier1: jtreg-tests-tier1

# for use with JPRT -testrule
all:		JTREG_TESTDIRS = .
jtreg:		JTREG_TESTDIRS = .
apt:		JTREG_TESTDIRS = tools/apt
javac: 		JTREG_TESTDIRS = tools/javac
javadoc:	JTREG_TESTDIRS = tools/javadoc com/sun/javadoc
javah:		JTREG_TESTDIRS = tools/javah
javap:		JTREG_TESTDIRS = tools/javap
jdeps:		JTREG_TESTDIRS = tools/jdeps

# Run jtreg tests
#
# JTREG_HOME
#	Installed location of jtreg
# JT_JAVA
#	Version of java used to run jtreg.  Should normally be the same as TESTJAVA
# TESTJAVA
# 	Version of java to be tested.
# JTREG_VERBOSE
# Verbosity setting for jtreg
# JTREG_OPTIONS
#	Additional options for jtreg
# JTREG_TESTDIRS
#	Directories of tests to be run
# JTREG_OUTPUT_DIR
#	Where to write the results
# JTREG_REFERENCE
#	(Optional) reference results (e.g. work, report or summary.txt)
#
jtreg_tests: jtreg-tests
jtreg-tests: check-jtreg FRC
	@rm -f -r $(JTREG_OUTPUT_DIR)/JTwork $(JTREG_OUTPUT_DIR)/JTreport \
	    $(JTREG_OUTPUT_DIR)/diff.html $(JTREG_OUTPUT_DIR)/status.txt
	@mkdir -p $(JTREG_OUTPUT_DIR)
	JT_JAVA=$(JT_JAVA) $(JTREG) \
	  -J-Xmx512m \
	  -vmoption:-Xmx768m \
	  -a -ignore:quiet $(if $(JTREG_VERBOSE),-v:$(JTREG_VERBOSE)) \
          -r:$(JTREG_OUTPUT_DIR)/JTreport \
          -w:$(JTREG_OUTPUT_DIR)/JTwork \
          -jdk:$(TESTJAVA) \
          $(JAVA_ARGS:%=-vmoption:%) \
	  $(JTREG_OPTIONS) \
	  $(JTREG_EXCLUSIONS) \
          $(JTREG_TESTDIRS) \
	|| ( $(call EXIT_IF_FATAL,$(FATAL_JTREG_EXIT)) ; \
	    echo $$status > $(JTREG_OUTPUT_DIR)/status.txt \
	)
ifdef JTREG_REFERENCE
	JT_JAVA=$(JT_JAVA) $(JTDIFF) -o $(JTREG_OUTPUT_DIR)/diff.html \
	    $(JTREG_REFERENCE) $(JTREG_OUTPUT_DIR)/JTreport \
	|| ( $(call EXIT_IF_FATAL,$(FATAL_JTDIFF_EXIT)) )
endif

jtreg-summary: FRC
	if [ -r $(JTREG_OUTPUT_DIR)/status.txt ]; then \
	    echo ; echo "Summary of jtreg test failures" ; \
	    cat $(JTREG_OUTPUT_DIR)/JTreport/text/summary.txt | \
		grep -v 'Not run' | grep -v 'Passed' ; \
	    echo ; \
	    $(EXIT) `cat $(JTREG_OUTPUT_DIR)/status.txt` ; \
	fi

# Check to make sure these directories exist
check-jtreg: $(PRODUCT_HOME) $(JTREG)

# Run JCK-compiler tests
#
# JCK_HOME
#	Installed location of JCK: should include JCK-compiler, and JCK-extras
#       Default is JCK 8.
# JT_JAVA
#	Version of java used to run JCK.  Should normally be the same as TESTJAVA
#       Default is JDK 7
# TESTJAVA
# 	Version of java to be tested.
# JCK_VERBOSE
#	Verbosity setting for jtjck
# JCK_COMPILER_OPTIONS
#	Additional options for JCK-compiler
# JCK_COMPILER_TESTDIRS
#	Directories of tests to be run
# JCK_COMPILER_OUTPUT_DIR
#	Where to write the results
# JCK_COMPILER_REFERENCE
#	(Optional) reference results (e.g. work, report or summary.txt)
#
jck-compiler-tests: check-jck FRC
	@rm -f -r $(JCK_COMPILER_OUTPUT_DIR)/work $(JCK_COMPILER_OUTPUT_DIR)/report \
	    $(JCK_COMPILER_OUTPUT_DIR)/diff.html $(JCK_COMPILER_OUTPUT_DIR)/status.txt
	@mkdir -p $(JCK_COMPILER_OUTPUT_DIR)
	$(JT_JAVA)/bin/java -Xmx512m \
	    -jar $(JCK_HOME)/JCK-compiler-8/lib/jtjck.jar \
	    $(if $(JCK_VERBOSE),-v:$(JCK_VERBOSE)) \
            -r:$(JCK_COMPILER_OUTPUT_DIR)/report \
            -w:$(JCK_COMPILER_OUTPUT_DIR)/work \
            -jdk:$(TESTJAVA) \
	    $(JCK_COMPILER_OPTIONS) \
            $(JCK_COMPILER_TESTDIRS) \
	|| ( $(call EXIT_IF_FATAL,$(FATAL_JCK_EXIT)) ; \
	    echo $$status > $(JCK_COMPILER_OUTPUT_DIR)/status.txt \
	)
ifdef JCK_COMPILER_REFERENCE
	JT_JAVA=$(JT_JAVA) $(JTDIFF) -o $(JCK_COMPILER_OUTPUT_DIR)/diff.html \
	    $(JCK_COMPILER_REFERENCE) $(JCK_COMPILER_OUTPUT_DIR)/report \
	|| ( $(call EXIT_IF_FATAL,$(FATAL_JTDIFF_EXIT)) )
endif

jck-compiler-summary: FRC
	if [ -r $(JCK_COMPILER_OUTPUT_DIR)/status.txt ]; then \
	    echo ; echo "Summary of JCK-compiler test failures" ; \
	    cat $(JCK_COMPILER_OUTPUT_DIR)/report/text/summary.txt | \
		grep -v 'Not run' | grep -v 'Passed' ; \
	    echo ; \
	    $(EXIT) `cat $(JCK_COMPILER_OUTPUT_DIR)/status.txt` ; \
	fi

# Run JCK-runtime tests in -Xcompile mode
# This is a special mode to test javac by compiling the tests in the JCK-runtime test suite
# Normal JCK-runtime invocation belongs in the jdk/ repository.
#
# JCK_HOME
#	Installed location of JCK: should include JCK-compiler, JCK-runtime and JCK-extras
# JT_JAVA
#	Version of java used to run JCK.  Should normally be the same as TESTJAVA
# TESTJAVA
# 	Version of java to be tested.
# JCK_VERBOSE
#	Verbosity setting for jtjck
# JCK_RUNTIME_OPTIONS
#	Additional options for JCK-runtime
# JCK_RUNTIME_TESTDIRS
#	Directories of tests to be run
# JCK_RUNTIME_OUTPUT_DIR
#	Where to write the results
# JCK_RUNTIME_REFERENCE
#	(Optional) reference results (e.g. work, report or summary.txt)
#
jck-runtime-tests: check-jck FRC
	@rm -f -r $(JCK_RUNTIME_OUTPUT_DIR)/work $(JCK_RUNTIME_OUTPUT_DIR)/report \
	    $(JCK_RUNTIME_OUTPUT_DIR)/diff.html $(JCK_RUNTIME_OUTPUT_DIR)/status.txt
	@mkdir -p $(JCK_RUNTIME_OUTPUT_DIR)
	$(JT_JAVA)/bin/java -Xmx512m \
	    -jar $(JCK_HOME)/JCK-runtime-8/lib/jtjck.jar \
	    $(if $(JCK_VERBOSE),-v:$(JCK_VERBOSE)) \
            -r:$(JCK_RUNTIME_OUTPUT_DIR)/report \
            -w:$(JCK_RUNTIME_OUTPUT_DIR)/work \
            -jdk:$(TESTJAVA) \
	    -Xcompile \
	    $(JCK_RUNTIME_OPTIONS) \
            $(JCK_RUNTIME_TESTDIRS) \
	|| ( $(call EXIT_IF_FATAL,$(FATAL_JCK_EXIT)) ; \
	    echo $$status > $(JCK_RUNTIME_OUTPUT_DIR)/status.txt \
	)
ifdef JCK_RUNTIME_REFERENCE
	JT_JAVA=$(JT_JAVA) $(JTDIFF) -o $(JCK_RUNTIME_OUTPUT_DIR)/diff.html \
	    $(JCK_RUNTIME_REFERENCE) $(JCK_RUNTIME_OUTPUT_DIR)/report \
	|| ( $(call EXIT_IF_FATAL,$(FATAL_JTDIFF_EXIT)) )
endif

jck-runtime-summary: FRC
	if [ -r $(JCK_RUNTIME_OUTPUT_DIR)/status.txt ]; then \
	    echo ; echo "Summary of JCK-runtime test failures" ; \
	    cat $(JCK_RUNTIME_OUTPUT_DIR)/report/text/summary.txt | \
		grep -v 'Not run' | grep -v 'Passed' ; \
	    echo ; \
	    $(EXIT) `cat $(JCK_RUNTIME_OUTPUT_DIR)/status.txt` ; \
	fi

# Check to make sure these directories exist
check-jck: $(JCK_HOME) $(PRODUCT_HOME)

all-summary: FRC
	if [ -n "`find $(TEST_OUTPUT_DIR) -name status.txt`" ]; then
	    echo ; echo "Summary of test failures" ; \
	    cat `find $(TEST_OUTPUT_DIR) -name summary.txt` | \
		grep -v 'Not run' | grep -v 'Passed' ; \
	    echo ; \
	    $(EXIT) 1
	fi

# Bundle up the results
$(JPRT_ARCHIVE_BUNDLE): FRC
	@rm -f $@
	@mkdir -p $(@D)
	( cd $(TEST_OUTPUT_DIR) && zip -q -r $@ . )

# Cleanup
clean:
	rm -f $(JPRT_ARCHIVE_BUNDLE)

# Used to force a target rules to run
FRC:

#
# Tier 1 langtools testing logic
#

ARCHIVE_BUNDLE = $(ABS_TEST_OUTPUT_DIR)/ARCHIVE_BUNDLE.zip

# How to create the test bundle (pass or fail, we want to create this)
#   Follow command with ";$(BUNDLE_UP_AND_EXIT)", so it always gets executed.
ZIP_UP_RESULTS = ( $(MKDIR) -p `$(DIRNAME) $(ARCHIVE_BUNDLE)`     \
	           && $(CD) $(ABS_TEST_OUTPUT_DIR)             \
	           && $(CHMOD) -R a+r . \
	           && $(ZIP) -q -r $(ARCHIVE_BUNDLE) . )

# important results files
SUMMARY_TXT = $(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/JTreport/text/summary.txt")
STATS_TXT_NAME = Stats.txt
STATS_TXT = $(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/$(STATS_TXT_NAME)")
RUNLIST   = $(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/runlist.txt")
PASSLIST  = $(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/passlist.txt")
FAILLIST  = $(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/faillist.txt")
EXITCODE  = $(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/exitcode.txt")

TESTEXIT = \
  if [ ! -s $(EXITCODE) ] ; then \
    $(ECHO) "ERROR: EXITCODE file not filled in."; \
    $(ECHO) "1" > $(EXITCODE); \
  fi ; \
  testExitCode=`$(CAT) $(EXITCODE)`; \
  $(ECHO) "EXIT CODE: $${testExitCode}"; \
  exit $${testExitCode}

BUNDLE_UP_AND_EXIT = \
( \
  jtregExitCode=$$? && \
  _summary="$(SUMMARY_TXT)"; \
  $(RM) -f $(STATS_TXT) $(RUNLIST) $(PASSLIST) $(FAILLIST) $(EXITCODE); \
  $(ECHO) "$${jtregExitCode}" > $(EXITCODE); \
  if [ -r "$${_summary}" ] ; then \
    $(ECHO) "Summary: $(UNIQUE_DIR)" > $(STATS_TXT); \
    $(EXPAND) $${_summary} | $(EGREP) -v ' Not run\.' > $(RUNLIST); \
    $(EGREP) ' Passed\.' $(RUNLIST) \
      | $(EGREP) -v ' Error\.' \
      | $(EGREP) -v ' Failed\.' > $(PASSLIST); \
    ( $(EGREP) ' Failed\.' $(RUNLIST); \
      $(EGREP) ' Error\.' $(RUNLIST); \
      $(EGREP) -v ' Passed\.' $(RUNLIST) ) \
      | $(SORT) | $(UNIQ) > $(FAILLIST); \
    if [ $${jtregExitCode} != 0 -o -s $(FAILLIST) ] ; then \
      $(EXPAND) $(FAILLIST) \
        | $(CUT) -d' ' -f1 \
        | $(SED) -e 's@^@FAILED: @' >> $(STATS_TXT); \
      if [ $${jtregExitCode} = 0 ] ; then \
        jtregExitCode=1; \
      fi; \
    fi; \
    runc="`$(CAT) $(RUNLIST)      | $(WC) -l | $(AWK) '{print $$1;}'`"; \
    passc="`$(CAT) $(PASSLIST)    | $(WC) -l | $(AWK) '{print $$1;}'`"; \
    failc="`$(CAT) $(FAILLIST)    | $(WC) -l | $(AWK) '{print $$1;}'`"; \
    exclc="FIXME CODETOOLS-7900176"; \
    $(ECHO) "TEST STATS: name=$(UNIQUE_DIR)  run=$${runc}  pass=$${passc}  fail=$${failc}" \
      >> $(STATS_TXT); \
  else \
    $(ECHO) "Missing file: $${_summary}" >> $(STATS_TXT); \
  fi; \
  if [ -f $(STATS_TXT) ] ; then \
    $(CAT) $(STATS_TXT); \
  fi; \
  $(ZIP_UP_RESULTS) ; \
  $(TESTEXIT) \
)

prep-tier1:
	@$(MKDIR) -p $(ABS_TEST_OUTPUT_DIR)

# Run tier1 jtreg
jtreg-tests-tier1: check-jtreg prep-tier1 FRC
	(                                                                    \
	  ( JT_HOME=$(shell $(GETMIXEDPATH) "$(JT_HOME)");                   \
            export JT_HOME;                                                  \
            $(shell $(GETMIXEDPATH) "$(JTREG)")                              \
	  -J-Xmx512m \
	  -vmoption:-Xmx768m \
	  -a -ignore:quiet $(if $(JTREG_VERBOSE),-v:$(JTREG_VERBOSE)) \
              -r:$(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/JTreport")  \
              -w:$(shell $(GETMIXEDPATH) "$(ABS_TEST_OUTPUT_DIR)/JTwork")    \
              -jdk:$(shell $(GETMIXEDPATH) "$(TESTJAVA)")                    \
	  $(JAVA_ARGS:%=-vmoption:%) \
	  $(JTREG_OPTIONS) \
	  $(JTREG_EXCLUSIONS) \
	  :langtools_tier1 \
	  ) ;                                                                \
	  $(BUNDLE_UP_AND_EXIT)                                              \
	) 2>&1 | $(TEE) $(ABS_TEST_OUTPUT_DIR)/output.txt ; $(TESTEXIT)

# Phony targets (e.g. these are not filenames)
.PHONY: all clean \
	jtreg javac javadoc javah javap jdeps jtreg-tests jtreg-summary check-jtreg \
	jck-compiler jck-compiler-tests jck-compiler-summary \
	jck-runtime jck-runtime-tests jck-runtime-summary check-jck prep-tier1 \
	jtreg-tests-tier1 tier1

# No use of suffix rules
.SUFFIXES:

