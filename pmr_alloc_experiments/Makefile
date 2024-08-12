DEBUG		:=	1

SOURCES		:=	main.cpp
LDLIBS		:=
BUILDDIR	:=	build
EXEC		:=	main

TARGET		:=	$(EXEC:%=$(BUILDDIR)/%)
OBJECTS		:=	$(SOURCES:%.cpp=$(BUILDDIR)/%.o)


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

.PHONY: clean $(BUILDDIR)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(LDLIBS)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)
	rm -fd $(BUILDDIR)
