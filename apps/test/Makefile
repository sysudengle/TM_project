CXX = g++
LDFLAGS = -lpthread -l_tm


LIBTM_DIR = ../..
OUT_DIR   = ../..



TEST_SRCS   = test.cc
TEST_OBJS   = $(TEST_SRCS:%.cc=%.o)

TEST2_SRCS   = test2.cc
TEST2_OBJS   = $(TEST2_SRCS:%.cc=%.o)

TEST_C_SRCS   = test_c.cc
TEST_C_OBJS   = $(TEST_C_SRCS:%.cc=%.o)

TEST_D_SRCS   = test_d.cc
TEST_D_OBJS   = $(TEST_D_SRCS:%.cc=%.o)

TEST_M_SRCS   = test_m.cc
TEST_M_OBJS   = $(TEST_M_SRCS:%.cc=%.o)

TEST_PS_SRCS   = test_ps.cc
TEST_PS_OBJS   = $(TEST_PS_SRCS:%.cc=%.o)





all:

test: $(TEST_OBJS)
	$(CXX) -o $(OUT_DIR)/$@ $(TEST_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

test2: $(TEST2_OBJS)
	$(CXX) -o $(OUT_DIR)/$@ $(TEST2_OBJS)  $(LDFLAGS) -L$(LIBTM_DIR)


test_c: $(TEST_C_OBJS)
	$(CXX) -o $(OUT_DIR)/$@ $(TEST_C_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

test_d: $(TEST_D_OBJS)
	$(CXX) -o $(OUT_DIR)/$@ $(TEST_D_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

	

test_m: $(TEST_M_OBJS)
	$(CXX) -o $(OUT_DIR)/$@  $(TEST_M_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)

test_ps: $(TEST_PS_OBJS)
	$(CXX) -o $(OUT_DIR)/$@ $(TEST_PS_OBJS)   $(LDFLAGS) -L$(LIBTM_DIR)





%.o: %.cc
	$(CXX) -c $(CFLAGS) -o $@ $<

%.o: %.c
	$(CXX) -c $(CFLAGS) -o $@ $<



$(BUILD_DIR)/%.o: %.cc
	$(CXX) -c $(CFLAGS) -o $@ $<



clean:
	rm -f *.o $(OUT_DIR)/test $(OUT_DIR)/test2
	rm -f $(OUT_DIR)/test_c $(OUT_DIR)/test_d
	rm -f $(OUT_DIR)/test_m $(OUT_DIR)/test_ps



	

