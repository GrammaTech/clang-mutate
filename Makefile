CXX := clang++
RTTIFLAG := -fno-rtti
CXXFLAGS := $(shell llvm-config --cxxflags) $(RTTIFLAG)
LLVMLDFLAGS := $(shell llvm-config --ldflags --libs) -ldl

SOURCES = clang-mutate.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXES = $(OBJECTS:.o=)
CLANGLIBS = \
	-lclangFrontend \
	-lclangSerialization \
	-lclangDriver \
	-lclangTooling \
	-lclangParse \
	-lclangSema \
	-lclangAnalysis \
	-lclangEdit \
	-lclangAST \
	-lclangLex \
	-lclangBasic \
	-lclangRewriteCore \
	-lclangRewriteFrontend

all: $(OBJECTS) $(EXES)
.PHONY: clean install

%: %.o
	$(CXX) -o $@ $< $(CLANGLIBS) $(LLVMLDFLAGS)

clang-mutate: ASTMutate.o ASTBinaryAddressLister.o clang-mutate.o
	$(CXX) -o $@ $^ $(CLANGLIBS) $(LLVMLDFLAGS)

clean:
	-rm -f $(EXES) $(OBJECTS) ASTMutate.o compile_commands.json a.out etc/hello etc/loop *~

install: clang-mutate
	cp $< $$(dirname $$(which clang))

# An alternative to giving compiler info after -- on the command line
compile_commands.json:
	echo -e "[\n\
	  {\n\
	    \"directory\": \"$$(pwd)\",\n\
	    \"command\": \"$$(which clang) $$(pwd)/etc/hello.c\",\n\
	    \"file\": \"$$(pwd)/etc/hello.c\"\n\
	  }\n\
	]\n" > $@
