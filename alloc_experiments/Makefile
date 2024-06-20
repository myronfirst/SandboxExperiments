DEBUG		:= 1

CXX			:=	g++-11
CXXFLAGS	:= \
-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -std=c++23
LDLIBS		:=	-lpmem -lpmemobj
SOURCES		:=	main.cpp interface.cpp interface.h
TARGETS		:=	\
build/bench_default build/bench_malloc build/bench_jemalloc \
build/bench_vmem build/bench_vmmalloc build/bench_memkind \
build/bench_pmemobj_alloc build/bench_make_persistent_atomic

ifeq ($(DEBUG),0)
	CXXFLAGS += -O3 -DNDEBUG
else
	CXXFLAGS += -O0
endif

.PHONY: clean

all: $(TARGETS)

build/bench_default: CXXFLAGS += -DDEFAULT
build/bench_default: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/bench_malloc: CXXFLAGS += -DMALLOC
build/bench_malloc: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/bench_jemalloc: CXXFLAGS += -DJEMALLOC
build/bench_jemalloc: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/bench_vmem: CXXFLAGS += -DVMEM
build/bench_vmem: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/bench_vmmalloc: CXXFLAGS += -DVMMALLOC
build/bench_vmmalloc: LDLIBS += -lvmmalloc
build/bench_vmmalloc: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/bench_memkind: CXXFLAGS += -DMEMKIND
build/bench_memkind: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/bench_pmemobj_alloc: CXXFLAGS += -DPMEMOBJ_ALLOC
build/bench_pmemobj_alloc: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/bench_make_persistent_atomic: CXXFLAGS += -DMAKE_PERSISTENT_ATOMIC
build/bench_make_persistent_atomic: $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(TARGETS)
