CXX = g++
LDFLAGS = -lpthread -l_tm
CFLAGS = -O3 -DVERSION_DYNAMIC
#-O2 -march=pentium-m -msse2 -mfpmath=sse

VERSION = -DVERSION_DYNAMIC

export VERSION

LIBTM_DIR = ../..
OUT_DIR   = ../..

MD_SRCS   = md.cc
MD_OBJS   = $(MD_SRCS:%.cc=%.o)

MMD_SRCS   = mmd.cc
MMD_OBJS   = $(MMD_SRCS:%.cc=%.o)

all:

md: clean $(MD_OBJS)
	$(CXX) -o $(OUT_DIR)/$@    $(MD_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

mmd: clean $(MMD_OBJS)
	$(CXX) -o $(OUT_DIR)/$@    $(MMD_OBJS)  $(LDFLAGS) -L$(LIBTM_DIR)

%.o: %.cc
	$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	-rm -f $(OUT_DIR)/md $(OUT_DIR)/mmd *.o
