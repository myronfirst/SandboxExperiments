DEBUG		:=	0

SOURCES		:=	main.cpp
LDLIBS		:=	-lstdc++ -pthread
BUILDDIR	:=	build
EXEC		:=	main

TARGET		:=	$(EXEC:%=$(BUILDDIR)/%)
OBJECTS		:=	$(SOURCES:%.cpp=$(BUILDDIR)/%.o)


CXX			:=	gcc-11
CXXFLAGS	:= \
-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -std=c++23

ifeq ($(DEBUG),3)
	CXXFLAGS += -O0 -fsanitize=thread
else ifeq ($(DEBUG),2)
	CXXFLAGS += -O0 -fsanitize=address,undefined,leak
else ifeq ($(DEBUG),1)
	CXXFLAGS += -O0
	# CXXFLAGS += -O0 #-D_GLIBCXX_DEBUG_PEDANTIC
else
	CXXFLAGS += -O3 -DNDEBUG
endif

.PHONY: clean $(BUILDDIR)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)
	rm -fd $(BUILDDIR)
