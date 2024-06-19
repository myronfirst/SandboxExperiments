CXX			:=	g++-11
CXXFLAGS	:=	-g -O0 -std=c++23 -D_GLIBCXX_DEBUG -Wall -Wextra -pedantic -pthread
# CXXFLAGS	:=	-O3 -std=c++23 -Wall -Wextra -pedantic -pthread -DNDEBUG
LFLAGS		:=	-lpmemobj
# LFLAGS		:=	-lpmem -lpmem2 -lpmempool -lpmemobj -lpmemlog -lpmemkv_json_config -lpmemkv -lpmemblk
TARGET		:=	cache_warm
OBJECTS		:=	$(TARGET).o Instrumenter.o

.PHONY: clean all
.default: all

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LFLAGS)

$(OBJECTS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(LFLAGS)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(TARGET)
