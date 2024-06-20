DEBUG		:= 1

CXX			:=	g++-11
CXXFLAGS	:= -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -std=c++23
LDLIBS		:=	-lpmem -lpmemobj
TARGETS		:=	bench_default bench_malloc

ifeq ($(DEBUG),0)
	CXXFLAGS += -O3 -DNDEBUG
else
	CXXFLAGS += -O0
endif

.PHONY: clean

all: $(TARGETS)

bench_default: CXXFLAGS += -DDEFAULT
bench_default: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

bench_malloc: CXXFLAGS += -DMALLOC
bench_malloc: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)



clean:
	$(RM) $(TARGETS)
