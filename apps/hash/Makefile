CXX = g++
LDFLAGS = -lpthread -l_tm
CFLAGS = -O3
#-O2 -march=pentium-m -msse2 -mfpmath=sse

LIBTM_DIR = ../..
OUT_DIR   = ../..

INCLUDES    = tm_hashtypes.h

HASH_SRCS   = tm_hash.cc
HASH_OBJS   = $(HASH_SRCS:%.cc=%.o)

all:

hash: clean $(HASH_OBJS)
	$(CXX) -o $(OUT_DIR)/$@   $(HASH_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

%.o: %.cc $(INCLUDES)
	$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	-rm -f $(OUT_DIR)/hash *.o
