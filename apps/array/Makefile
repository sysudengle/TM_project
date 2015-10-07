####################################################################################################
##
## CONFIGURATION
##
####################################################################################################

## Compilation parameters
CC = gcc
CXX = g++

ifeq ($(DEBUG), )
	DEBUG = -O0
endif

#Application options
VERSION = -DTM -DTIMER

CXXFLAGS := -Wall $(DEBUG) $(VERSION) $(OPTIONS) -DmwPC  -mrtm -DTM_STATS -DEAGER_ABORT
CFLAGS := -Wall $(DEBUG) $(VERSION) $(OPTIONS) -DmwPC  -mrtm -DTM_STATS -DEAGER_ABORT
LDFLAGS = -lpthread -lstdc++ -l_tm


## Source and binary files
SRCS      =   $(wildcard *.cc)
OBJS      =   $(patsubst %.cc,%.o,$(SRCS))

LIBTM_DIR =		../..
OUT_DIR		=		../..
TARGET		=		array_tm

$(OBJS)		:	$(SRCS) Makefile

$(TARGET)	:	$(OBJS)
						$(CXX) $(CFLAGS) -o $(OUT_DIR)/$@ $(OBJS) $(LDFLAGS) -L$(LIBTM_DIR)

default		: $(TARGET)

clean:
		-rm -f $(OBJS) $(OUT_DIR)/$(TARGET)

.PHONY: clean
