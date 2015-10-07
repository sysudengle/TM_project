CXX = g++
LDFLAGS = -lpthread -l_tm

LIBTM_DIR = ../..
OUT_DIR   = ../..

BENCH_SRCS  = $(wildcard ./*.cc)
BENCH_OBJS  = $(BENCH_SRCS:%.cc=%.o)

all:

bench: clean $(BENCH_OBJS)
	$(CXX) $(CFLAGS) -o $(OUT_DIR)/$@   $(BENCH_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

%.o: %.cc
	$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	-rm -f $(OUT_DIR)/bench *.o
