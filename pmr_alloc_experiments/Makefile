DEBUG		:=	0

SOURCE_DIR  :=  src

SOURCES		:=	$(wildcard $(SOURCE_DIR)/*.c) $(wildcard $(SOURCE_DIR)/*.cpp)
LDLIBS		:=	-lstdc++ -pthread
BUILDDIR	:=	build
EXEC		:=	main

TARGET		:=	$(EXEC:%=$(BUILDDIR)/%)
OBJECTS := $(SOURCES:$(SOURCE_DIR)/%.cpp=$(BUILDDIR)/%.o)
OBJECTS := $(OBJECTS:$(SOURCE_DIR)/%.c=$(BUILDDIR)/%.o)


CXX			:=	gcc-11
CXXFLAGS	:= \
-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -Iincludes -DSYNCH_DISABLE_PWBS -DSYNCH_DISABLE_PSYNCS

JE 			:= -L`jemalloc-config --libdir` -Wl,-rpath,`jemalloc-config --libdir` -ljemalloc `jemalloc-config --libs`


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
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS) $(JE)

$(BUILDDIR)/%.o: $(SOURCE_DIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -std=c++23 -c -o $@ $<

$(BUILDDIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -std=gnu89 -c -o $@ $<


$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)
	rm -fd $(BUILDDIR)
