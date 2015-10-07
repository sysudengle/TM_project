CXX = g++
LDFLAGS = -lpthread -lm -l_tm
CFLAGS = -O3 # -ggdb
#-O2 -march=pentium-m -msse2 -mfpmath=sse

LIBTM_DIR = ../..
OUT_DIR   = ../..

QS_SRCS   = qs.cc
QS_OBJS   = $(QS_SRCS:%.cc=%.o)

all:

qs: clean $(QS_OBJS)
	$(CXX) -o $(OUT_DIR)/$@   $(QS_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

%.o: %.cc
	$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	-rm -f $(OUT_DIR)/qs *.o

