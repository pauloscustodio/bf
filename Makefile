# Brainfuck interpreter

ifeq ($(OS),Windows_NT)
  _EXE := .exe
else
  _EXE ?=
endif

CXX			?= g++
CXXFLAGS	+= -std=gnu++17 -MMD -Wall -Wextra -Werror -pedantic-errors \
			   -I src/common
ASTYLE		= astyle --style=attach --pad-oper --align-pointer=type \
		      --break-closing-braces --add-braces --attach-return-type \
		      --max-code-length=120 --lineend=linux --formatted \
		      --recursive "*.cpp" "*.h"

BF_SRCS		= $(wildcard src/bf/*.cpp)
BF_OBJS		= $(BF_SRCS:.cpp=.o)

BFPP_SRCS	= $(wildcard src/bfpp/*.cpp)
BFPP_OBJS	= $(BFPP_SRCS:.cpp=.o)

BFBASIC_SRCS= $(wildcard src/bfbasic/*.cpp)
BFBASIC_OBJS= $(BFBASIC_SRCS:.cpp=.o)

COMMON_SRCS	= $(wildcard src/common/*.cpp)
COMMON_OBJS	= $(COMMON_SRCS:.cpp=.o)

DEPENDS		= $(BF_SRCS:.cpp=.d) \
			  $(BFPP_SRCS:.cpp=.d) \
			  $(BFBASIC_SRCS:.cpp=.d) \
			  $(COMMON_SRCS:.cpp=.d)

all: bf$(_EXE) bfpp$(_EXE) bfbasic$(_EXE)

bf$(_EXE): $(BF_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(BF_OBJS)

bfpp$(_EXE): $(BFPP_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(BFPP_OBJS) $(COMMON_OBJS)

bfbasic$(_EXE): $(BFBASIC_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(BFBASIC_OBJS) $(COMMON_OBJS)

astyle:
	$(ASTYLE)

clean:
	$(RM) bf$(_EXE)   $(BF_OBJS) \
	      bfpp$(_EXE) $(BFPP_OBJS) \
		  bfbasic$(_EXE) $(BFBASIC_OBJS) \
		  $(COMMON_OBJS) $(DEPENDS) \
		  $(foreach dir,src/bf src/bfpp src/bfbasic,$(wildcard $(dir)/*.orig))

test: bf$(_EXE) bfpp$(_EXE) bfbasic$(_EXE)
	perl -S prove -j9 --state=slow,save $(wildcard t/*.t)

-include $(DEPENDS)
