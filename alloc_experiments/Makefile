DEBUG		:= 0

SOURCES		:=	main.cpp interface.cpp
LDLIBS		:=	-pthread -lpmem -lpmemobj -lvmem -lmemkind
BUILDDIR	:=	build
TARGETS		:=	\
$(BUILDDIR)/bench_default $(BUILDDIR)/bench_malloc $(BUILDDIR)/bench_jemalloc \
$(BUILDDIR)/bench_vmem $(BUILDDIR)/bench_vmmalloc $(BUILDDIR)/bench_memkind \
$(BUILDDIR)/bench_pmemobj_alloc $(BUILDDIR)/bench_make_persistent_atomic \
$(BUILDDIR)/bench_pmem


CC			:=	g++-11
CCFLAGS		:= \
-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -std=c++23


ifeq ($(DEBUG),0)
	CCFLAGS += -O3 -DNDEBUG
else
	CCFLAGS += -O0
endif

.PHONY: clean

all: $(TARGETS)

$(BUILDDIR)/bench_default: CCFLAGS += -DBENCH_DEFAULT
$(BUILDDIR)/bench_default: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_malloc: CCFLAGS += -DBENCH_MALLOC
$(BUILDDIR)/bench_malloc: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_jemalloc: CCFLAGS += -DBENCH_JEMALLOC
# $(BUILDDIR)/bench_jemalloc: LDLIBS += -L`jemalloc-config --libdir` -Wl,-rpath,`jemalloc-config --libdir` -ljemalloc `jemalloc-config --libs`
$(BUILDDIR)/bench_jemalloc: LDLIBS += -ljemalloc
$(BUILDDIR)/bench_jemalloc: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_vmem: CCFLAGS += -DBENCH_VMEM
$(BUILDDIR)/bench_vmem: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_vmmalloc: CCFLAGS += -DBENCH_VMMALLOC
$(BUILDDIR)/bench_vmmalloc: LDLIBS += -lvmmalloc
$(BUILDDIR)/bench_vmmalloc: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_memkind: CCFLAGS += -DBENCH_MEMKIND
$(BUILDDIR)/bench_memkind: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_pmemobj_alloc: CCFLAGS += -DBENCH_PMEMOBJ_ALLOC
$(BUILDDIR)/bench_pmemobj_alloc: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_make_persistent_atomic: CCFLAGS += -DBENCH_MAKE_PERSISTENT_ATOMIC
$(BUILDDIR)/bench_make_persistent_atomic: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/bench_pmem: CCFLAGS += -DBENCH_PMEM
$(BUILDDIR)/bench_pmem: $(SOURCES) | $(BUILDDIR)
	$(CC) $(CCFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(TARGETS)
