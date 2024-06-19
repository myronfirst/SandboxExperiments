DEBUG := 1

CXX			:=	g++-11
CXXFLAGS	:=	-g3 -std=c++23 -Wall -Wextra -pedantic
# CXXFLAGS	:=	-O3 -std=c++23 -Wall -Wextra -pedantic -pthread -DNDEBUG
LFLAGS		:=	-lpmem -lpmemobj
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
