# Brainfuck interpreter

ifeq ($(OS),Windows_NT)
  _EXE := .exe
else
  _EXE ?=
endif

CXX			?= g++
CXXFLAGS	+= -std=gnu++17 -MMD -Wall -Wextra -Werror -pedantic-errors
ASTYLE		= astyle --style=attach --pad-oper --align-pointer=type \
		      --break-closing-braces --add-braces --attach-return-type \
		      --max-code-length=120 --lineend=linux --formatted \
		      --recursive "*.cpp" "*.h"

BF_SRCS		= $(wildcard src/bf/*.cpp)
BF_OBJS		= $(BF_SRCS:.cpp=.o)
BFPP_SRCS	= $(wildcard src/bfpp/*.cpp)
BFPP_OBJS	= $(BFPP_SRCS:.cpp=.o)
DEPENDS		= $(BF_SRCS:.cpp=.d) $(BFPP_SRCS:.cpp=.d)

all: bf$(_EXE) bfpp$(_EXE)

bf$(_EXE): $(BF_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(BF_OBJS)

bfpp$(_EXE): $(BFPP_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(BFPP_OBJS)

astyle:
	$(ASTYLE)

clean:
	$(RM) bf$(_EXE) $(BF_OBJS) $(BFPP_OBJS) $(DEPENDS) \
		$(foreach dir,src/bf src/bfpp,$(wildcard $(dir)/*.orig))

test: bf$(_EXE) bfpp$(_EXE)
	perl -S prove $(wildcard t/*.t)

-include $(DEPENDS)
