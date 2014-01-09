
CXX = g++
#CPPFLAGS = -DF4M_DEBUG
CXXFLAGS = -fPIC -Wall -Wextra -std=c++11 -fvisibility=hidden
LDFLAGS = -lpugixml -shared
RM = rm -f
TARGET_LIB = libf4mparser.so

SRCS = f4mparser/urlutils.cpp f4mparser/manifestparserhelper.cpp f4mparser/manifestparser.cpp f4mparser/manifestdoc.cpp f4mparser/f4mparser.cpp  f4mparser/base64utils.cpp

OBJS = $(SRCS:.cpp=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CXX) -o $@ $^ ${LDFLAGS}

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS}
