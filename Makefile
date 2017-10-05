BASEDIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

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
CLANG = $(LLVM_HOME)clang++$(LLVM_POSTFIX)
REPO ?= clang-mutate
BRANCH ?= master
LLVM_INCS ?= -I ../third-party/llvm/include/
RTTIFLAG := -fno-rtti
PICOJSON_INCS := -I third-party/picojson-1.3.0
PICOJSON_DEFINES := -D PICOJSON_USE_INT64
ELFIO_INCS := -I third-party/elfio-3.2
LLVM_CONFIG := $(LLVM_HOME)llvm-config$(LLVM_POSTFIX)
LLVM_DWARFDUMP ?= llvm-dwarfdump
CXXFLAGS := -Wno-unknown-warning-option $(shell $(LLVM_CONFIG) --cxxflags) -I. $(RTTIFLAG) $(PICOJSON_INCS) $(PICOJSON_DEFINES) $(ELFIO_INCS) $(LLVM_INCS) -DLLVM_DWARFDUMP='"$(LLVM_DWARFDUMP)"'
LLVMLDFLAGS := $(shell $(LLVM_CONFIG) --ldflags --libs) -ldl

SOURCES = Rewrite.cpp EditBuffer.cpp SyntacticContext.cpp Interactive.cpp Function.cpp Variable.cpp Ast.cpp TU.cpp Requirements.cpp Bindings.cpp Renaming.cpp Scopes.cpp Macros.cpp TypeDBEntry.cpp AuxDB.cpp BinaryAddressMap.cpp LLVMInstructionMap.cpp Json.cpp Utils.cpp Cfg.cpp clang-mutate.cpp
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

JSHON_DIR = third-party/jshon
JANSSON_DIR = third-party/jansson
JSHON_BIN = $(JSHON_DIR)/jshon
JANSSON_LIB = $(JANSSON_DIR)/lib/libjansson.a
GTR_DIR = third-party/gtr

all: $(EXES)
.PHONY: clean install tests.md auto-check man doc

%: %.o
	$(CXX) -o $@ $<

clang-mutate: $(OBJECTS)
	$(CXX) -o $@ $^ $(CLANGLIBS) $(LLVMLDFLAGS) $(SYSLIBS)

man doc:
	make -C man

man/clang-mutate.1.gz: man/clang-mutate.template.md
	make -C man clang-mutate.1.gz

.PHONY: clean
clean:
	-rm -f $(EXES) $(OBJECTS) a.out etc/hello etc/hello.ll etc/loop *~

.PHONY: real-clean
real-clean: clean
	-rm -f $(JSHON_BIN) $(JANSSON_LIB)
	-$(MAKE) -C $(JANSSON_DIR) clean
	-$(MAKE) -C $(JSHON_DIR) clean

install: clang-mutate man/clang-mutate.1.gz
	install -Dm755 clang-mutate $$(dirname $$(which clang$(LLVM_POSTFIX)))
	install -Dm644 man/clang-mutate.1.gz $$(dirname $$(dirname $$(which clang$(LLVM_POSTFIX))))/share/man/man1/

# This target builds an Arch package from the current state of the
# repository by first rsync'ing it into the makepkg source directory
# then running makepkg to build a package.
local-makepkg:
	-rsync --exclude .git --exclude src/clang-mutate_pkg -aruv ./ src/clang-mutate_pkg
	make -C src/clang-mutate_pkg clean
	makepkg -ef


# Test prerequisite binaries
$(JANSSON_LIB):
	cd $(JANSSON_DIR) && \
	autoreconf -i && \
	./configure --prefix=$(BASEDIR)/$(JANSSON_DIR) && \
	$(MAKE) && \
	$(MAKE) install

.PHONY: jannson
jansson: $(JANSSON_LIB)

$(JSHON_BIN): $(JANSSON_LIB)
	cd $(JSHON_DIR) && $(MAKE)

.PHONY: jshon
jshon: $(JSHON_BIN)


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
	hello-json-llvm-ir-default-fields		\
	hello-json-llvm-ir-number-of-stmts-w-llvm-ir	\
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
	null-finds-stddef                               \
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
	array-of-pointers-has-type			\
	pointer-to-array-has-type			\
	segmentation-fault				\
	replace-var-in-macro				\
	lighttpd-bug-does-not-crash			\
	pound-define-func-test				\
	pound-define-struct-test			\
	function-bodies-have-children                \
	segmentation-fault				\
	type-has-name-hash				\
	function-body-top-level-syntax			\
	type-function-arg-hash				\
	type-function-body-hash				\
	type-function-ret-hash				\
	typedef-type-hash-test				\
	full-stmt-in-switch                             \
	cxx-method-has-body                             \
	cxx-method-body-is-full-stmt			\
	multidimensional-array-has-type			\
	global-var-has-type				\
	for-loop-body-is-full-statement			\
	while-loop-body-is-full-statement		\
	do-loop-body-is-full-statement			\
	if-body-is-full-statement                       \
	array-initializer-has-correct-children 		\
    operator-call-expr-ranges                       \
    type-sizes-are-correct \
    global-vars-syn-ctx \
    pointer-to-pointer-no-decl \
    struct-in-sys-header \
    function-in-sys-header \
    sqrt-math-header-found \
    open-stat-header-found \
    macro-with-var-decl \
    macro-with-multivar-decl \
    const-var \
    volatile-var \
    restrict-var \
    function-pointer \
    function-pointer-no-proto \
    int64_t-finds-header \
    alloca-macro \
    storage-classes

etc/hello: etc/hello.c
	$(CXX) -g -O0 $< -o $@

etc/hello.ll: etc/hello.c
	$(CLANG) -S -emit-llvm -g -O0 $< -o $@

PASS=\e[1;1m\e[1;32mPASS\e[1;0m
FAIL=\e[1;1m\e[1;31mFAIL\e[1;0m
check/%: test/% etc/hello etc/hello.ll $(JSHON_BIN)
	@if ./$< >/dev/null 2>/dev/null;then \
	printf "$(PASS)\t"; \
	else \
	printf "$(FAIL)\t"; \
	fi
	@printf "\e[1;1m%s\e[1;0m\n" $*

testbot-check/%: test/% etc/hello etc/hello.ll $(JSHON_BIN)
	@XML_FILE=$$(mktemp /tmp/clang-mutate-tests.XXXXX); \
	printf "<test_run>\n" >> $$XML_FILE; \
	printf "  <name>$*</name>\n" >> $$XML_FILE; \
	printf "  <host>$$HOSTNAME</host>\n" $* >> $$XML_FILE; \
	printf "  <branch>$(BRANCH)</branch>\n" >> $$XML_FILE; \
	printf "  <project>$(REPO)</project>\n" >> $$XML_FILE; \
	printf "  <genus>regressions</genus>\n" >> $$XML_FILE; \
	printf "  <date_time>$$(date +%Y-%m-%dT%H:%M:%S)</date_time>\n" >> $$XML_FILE; \
	printf "  <key_value>\n" >> $$XML_FILE; \
	printf "    <key>User</key>\n" >> $$XML_FILE; \
	printf "    <text>$$USER</text>\n" >> $$XML_FILE; \
	printf "  </key_value>\n" >> $$XML_FILE; \
	printf "  <key_value>\n" >> $$XML_FILE; \
	printf "    <key>Result</key>\n" >> $$XML_FILE; \
	./$< >/dev/null 2>/dev/null; \
	EXIT_CODE=$$?; \
	if [ $$EXIT_CODE -eq 0 ];then \
	printf "    <text>Pass</text>\n" >> $$XML_FILE; \
	printf "    <number>5</number>\n" >> $$XML_FILE; \
	else \
	printf "    <text>Fail</text>\n" >> $$XML_FILE; \
	printf "    <number>1</number>\n" >> $$XML_FILE; \
	fi; \
	printf "  </key_value>\n" >> $$XML_FILE; \
	printf "</test_run>\n" >> $$XML_FILE; \
	python $(BASEDIR)/third-party/gtr/gtnetcat.py datamanager 55555 $$XML_FILE \
			>/dev/null 2>/dev/null; \
	rm $$XML_FILE; \
	exit $$EXIT_CODE;

desc/%: check/%
	@test/$* -d

check: $(addprefix check/, $(TESTS))
check-testbot: $(addprefix testbot-check/, $(TESTS))
check-desc: $(addprefix desc/, $(TESTS))

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
