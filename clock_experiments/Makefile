DEBUG		:= 1

SOURCES		:=	main.cpp
LDLIBS		:=
BUILDDIR	:=	build
TARGETS		:=	$(BUILDDIR)/main

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

$(BUILDDIR)/main: $(SOURCES) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR):
	mkdir -p $@

clean:
	$(RM) $(TARGETS)
