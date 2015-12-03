CXX := clang++
RTTIFLAG := -fno-rtti
PICOJSON_INCS := -I third-party/picojson-1.3.0
PICOJSON_DEFINES := -D PICOJSON_USE_INT64
CXXFLAGS := $(shell llvm-config --cxxflags) $(RTTIFLAG) $(PICOJSON_INCS) $(PICOJSON_DEFINES)
LLVMLDFLAGS := $(shell llvm-config --ldflags --libs) -ldl

SOURCES = ASTMutate.cpp ASTLister.cpp ASTEntry.cpp ASTEntryList.cpp Bindings.cpp Renaming.cpp Scopes.cpp Macros.cpp BinaryAddressMap.cpp clang-mutate.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXES = clang-mutate
CLANGLIBS = \
	-lpthread \
	-lz \
	-ltinfo \
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
	-lclangRewrite

all: $(EXES)
.PHONY: clean install

%: %.o
	$(CXX) -o $@ $< 

clang-mutate: $(OBJECTS)
	$(CXX) -o $@ $^ $(CLANGLIBS) $(LLVMLDFLAGS)

clean:
	-rm -f $(EXES) $(OBJECTS) compile_commands.json a.out etc/hello etc/loop *~

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
