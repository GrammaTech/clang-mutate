
# Set personal or machine-local flags in a file named local.mk
ifneq ("$(wildcard local.mk)","")
include local.mk
endif

# Ensure LLVM_HOME ends in a "/".
ifneq ("$(LLVM_HOME)","")
ifneq ("$(LLVM_HOME)","$(dir $(LLVM_HOME))")
	LLVM_HOME:=$(LLVM_HOME)/
endif
endif

ifeq "$(origin CXX)" "default"
CXX = $(LLVM_HOME)clang++$(LLVM_POSTFIX)
endif
RTTIFLAG := -fno-rtti
PICOJSON_INCS := -I third-party/picojson-1.3.0
PICOJSON_DEFINES := -D PICOJSON_USE_INT64
LLVM_CONFIG := $(LLVM_HOME)llvm-config$(LLVM_POSTFIX)
LLVM_DWARFDUMP := $(LLVM_HOME)llvm-dwarfdump$(LLVM_POSTFIX)
CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags) -I. $(RTTIFLAG) $(PICOJSON_INCS) $(PICOJSON_DEFINES) -DLLVM_DWARFDUMP='"$(LLVM_DWARFDUMP)"'
LLVMLDFLAGS := $(shell $(LLVM_CONFIG) --ldflags --libs) -ldl

SOURCES = Rewrite.cpp EditBuffer.cpp SyntacticContext.cpp Interactive.cpp Function.cpp Variable.cpp Ast.cpp TU.cpp Requirements.cpp Bindings.cpp Renaming.cpp Scopes.cpp Macros.cpp TypeDBEntry.cpp AuxDB.cpp BinaryAddressMap.cpp Json.cpp Utils.cpp Cfg.cpp clang-mutate.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXES = clang-mutate
SYSLIBS = \
	-lpthread \
	-lz \
	-ltinfo

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
	-lclangRewrite

all: $(EXES)
.PHONY: clean install tests.md auto-check

%: %.o
	$(CXX) -o $@ $<

clang-mutate: $(OBJECTS)
	$(CXX) -o $@ $^ $(CLANGLIBS) $(LLVMLDFLAGS) $(SYSLIBS)

clean:
	-rm -f $(EXES) $(OBJECTS) compile_commands.json a.out etc/hello etc/loop *~

install: clang-mutate
	cp $< $$(dirname $$(which clang$(LLVM_POSTFIX)))

# An alternative to giving compiler info after -- on the command line
compile_commands.json:
	echo -e "[\n\
	  {\n\
	    \"directory\": \"$$(pwd)\",\n\
	    \"command\": \"$$(which clang) $$(pwd)/etc/hello.c\",\n\
	    \"file\": \"$$(pwd)/etc/hello.c\"\n\
	  }\n\
	]\n" > $@


# Tests
TESTS =	help-text-appears				\
	hello-second-stmt-says-hello-json		\
	hello-json-list-size                            \
	hello-json-list-size-with-stmt1-filter          \
	hello-json-default-fields                       \
	hello-json-specify-fields                       \
	hello-json-entries-in-counter-order		\
	hello-json-bin-default-fields			\
	hello-json-bin-number-of-stmts-w-binary-data	\
	hello-set2-w-values				\
	hello-no-semi-colon-on-end-of-statement-json	\
	hello-semi-colon-on-end-of-statement-insert	\
	hello-semi-colon-on-end-of-statement-swap	\
	hello-semi-colon-on-end-of-value-insert		\
	hello-semi-colon-on-end-of-set			\
	hello-semi-colon-on-end-of-set2			\
	hello-semi-colon-on-end-of-statement-cut	\
	hello-semi-colon-on-end-of-statement-cut        \
	hello-finds-stdio				\
	decls-are-found                                 \
	short-macro-is-found                            \
	long-macro-is-found                             \
	no-aux-gives-all-entries                        \
	aux-includes-expected-entries                   \
	aux-excludes-expected-entries                   \
	null-finds-stdlib                               \
	null-has-no-macros				\
	scopes-block-starts-scope			\
	scopes-function-args-are-local			\
	scopes-function-body-starts-scope               \
	parmvar-has-declares				\
	list-init-expr-not-full-stmt			\
	wrap-then-branch-in-curlies			\
	wrap-do-body-in-curlies				\
	replace-then-branch-keeps-curlies 		\
	replace-then-branch-keeps-semicolon		\
	macro-src-text-is-correct			\
	replace-in-macro-expansion			\
	recursive-type-does-not-crash			\
	find-char-star-type				\
	segmentation-fault				\
	replace-var-in-macro

etc/hello: etc/hello.c
	$(CC) -g -O0 $< -o $@

PASS=\e[1;1m\e[1;32mPASS\e[1;0m
FAIL=\e[1;1m\e[1;31mFAIL\e[1;0m
check/%: test/% etc/hello
	@if ./$< >/dev/null 2>/dev/null;then \
	printf "$(PASS)\t"; \
	else \
	printf "$(FAIL)\t"; \
	fi
	@printf "\e[1;1m%s\e[1;0m\n" $*

desc/%: check/%
	@test/$* -d

check: $(addprefix check/, $(TESTS))
desc-check: $(addprefix desc/, $(TESTS))

# Makefile target to support automated testing.
tests.md: clang-mutate
	echo "### $$(date +%Y-%m-%d-%H-%M-%S)" >> tests.md
	echo "REPO" >> tests.md
	echo ":   $(REPO)" >> tests.md
	echo "" >> tests.md
	echo "BRANCH" >> tests.md
	echo ":   $(BRANCH)" >> tests.md
	echo "" >> tests.md
	echo "HEAD" >> tests.md
	echo ":   $(HEAD)" >> tests.md
	echo "" >> tests.md
	make -s check|sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[m|K]//g" \
		|sed -r "s/\\\e\[1;[0-9]*m//g" \
		|sed 's|^FAIL|<span style="color:red;font-family:monospace;">FAIL</span>|' \
		|sed 's|^PASS|<span style="color:green;font-family:monospace">PASS</span>|' \
		|sed 's/^/- /' >> tests.md
	echo "" >> tests.md

tests.html: tests.md
	markdown $< > $@

auto-check: tests.html
