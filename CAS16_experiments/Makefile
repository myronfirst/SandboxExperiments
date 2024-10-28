DEBUG		:=	0
PARAMS		:= -UPARAM_N_THREADS -UPARAM_CAS2_IMPL
# PARAMS		:= -DPARAM_N_THREADS=24 -DPARAM_CAS2_IMPL=LibAtomic

SOURCE_DIR  :=  src
BUILDDIR	:=	build
EXEC		:=	main
LDLIBS		:=	-lstdc++ -pthread -latomic

SOURCES		:=	$(wildcard $(SOURCE_DIR)/*.cpp)
TARGET		:=	$(EXEC:%=$(BUILDDIR)/%)
OBJECTS		:=	$(SOURCES:$(SOURCE_DIR)/%.cpp=$(BUILDDIR)/%.o)

CC			:=	gcc-11
CCFLAGS		:= \
-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -std=c++23 -march=native

ifeq ($(DEBUG),3)
	CCFLAGS += -O0 -fsanitize=thread
else ifeq ($(DEBUG),2)
	CCFLAGS += -O0 -fsanitize=address,undefined,leak
else ifeq ($(DEBUG),1)
	CCFLAGS += -O0
	# CCFLAGS += -O0 #-D_GLIBCXX_DEBUG_PEDANTIC
else
	CCFLAGS += -O3 -DNDEBUG
endif

.PHONY: clean $(BUILDDIR)

all: $(TARGET)

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CCFLAGS) $(PARAMS) -o $@ $^ $(LDLIBS)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/main.o: $(SOURCE_DIR)/main.cpp | $(BUILDDIR)
	$(CC) $(CCFLAGS) $(PARAMS) -c -o $@ $<
