# Brainfuck interpreter

ifeq ($(OS),Windows_NT)
  _EXE := .exe
else
  _EXE ?=
endif

CXX			?= g++
CXXFLAGS	+= -std=gnu++17 -MMD -Wall -Wextra -Werror -pedantic-errors
SRCS		= $(wildcard *.cpp)
OBJS		= $(SRCS:.cpp=.o)
DEPENDS		= $(SRCS:.cpp=.d)
ASTYLE		= astyle --style=attach --pad-oper --align-pointer=type \
		      --break-closing-braces --add-braces --attach-return-type \
		      --max-code-length=120 --lineend=linux --formatted \
		      --recursive "*.cpp" "*.h"

all: bf$(_EXE)

bf$(_EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

astyle:
	$(ASTYLE)

clean:
	$(RM) bf$(_EXE) $(OBJS) $(DEPENDS) $(wildcard *.orig)

test: bf$(_EXE)
	perl -S prove $(wildcard t/*.t)

-include $(DEPENDS)
