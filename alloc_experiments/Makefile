DEBUG		:= 1

SOURCES		:=	main.cpp interface.cpp
LDLIBS		:=	-pthread -lpmem -lpmemobj -lvmem -lmemkind
BUILDDIR	:=	build
TARGETS		:=	\
$(BUILDDIR)/bench_default $(BUILDDIR)/bench_malloc $(BUILDDIR)/bench_jemalloc \
$(BUILDDIR)/bench_vmem $(BUILDDIR)/bench_vmmalloc $(BUILDDIR)/bench_memkind \
$(BUILDDIR)/bench_pmemobj_alloc $(BUILDDIR)/bench_make_persistent_atomic \
$(BUILDDIR)/bench_pmem


CXX			:=	g++-11
CXXFLAGS	:= \
-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -std=c++23


ifeq ($(DEBUG),0)
	CXXFLAGS += -O3 -DNDEBUG
else
	CXXFLAGS += -O0
endif

.PHONY: clean

all: $(TARGETS)

$(BUILDDIR)/bench_default: CXXFLAGS += -DBENCH_DEFAULT
$(BUILDDIR)/bench_default: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_malloc: CXXFLAGS += -DBENCH_MALLOC
$(BUILDDIR)/bench_malloc: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_jemalloc: CXXFLAGS += -DBENCH_JEMALLOC
# $(BUILDDIR)/bench_jemalloc: LDLIBS += -L`jemalloc-config --libdir` -Wl,-rpath,`jemalloc-config --libdir` -ljemalloc `jemalloc-config --libs`
$(BUILDDIR)/bench_jemalloc: LDLIBS += -ljemalloc
$(BUILDDIR)/bench_jemalloc: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_vmem: CXXFLAGS += -DBENCH_VMEM
$(BUILDDIR)/bench_vmem: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_vmmalloc: CXXFLAGS += -DBENCH_VMMALLOC
$(BUILDDIR)/bench_vmmalloc: LDLIBS += -lvmmalloc
$(BUILDDIR)/bench_vmmalloc: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_memkind: CXXFLAGS += -DBENCH_MEMKIND
$(BUILDDIR)/bench_memkind: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_pmemobj_alloc: CXXFLAGS += -DBENCH_PMEMOBJ_ALLOC
$(BUILDDIR)/bench_pmemobj_alloc: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_make_persistent_atomic: CXXFLAGS += -DBENCH_MAKE_PERSISTENT_ATOMIC
$(BUILDDIR)/bench_make_persistent_atomic: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_pmem: CXXFLAGS += -DBENCH_PMEM
$(BUILDDIR)/bench_pmem: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(TARGETS)
