DEBUG := 1

CXX			:=	g++-11
CXXFLAGS	:= -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic \
-Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion \
-Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches \
-Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 \
-g3 -std=c++23
# CXXFLAGS	:=	-O3 -std=c++23 -Wall -Wextra -pedantic -pthread -DNDEBUG
LFLAGS		:=	-lpmem -lpmemobj -lvmmalloc
# LFLAGS		:=	-lpmem -lpmem2 -lpmempool -lpmemobj -lpmemlog -lpmemkv_json_config -lpmemkv -lpmemblk
TARGET		:=	main

ifeq ($(DEBUG),0)
	CXXFLAGS += -O3 -DNDEBUG
else
	CXXFLAGS += -O0
endif

.PHONY: clean

all: $(TARGET)

$(TARGET): $(TARGET).cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LFLAGS)

clean:
	$(RM) $(TARGET)
