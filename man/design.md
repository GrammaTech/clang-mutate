% CLANG_MUTATE Design
% GrammaTech Inc.
% February 4, 2016

A discussion of the design of `clang-mutate` and it's use of Clang and
LibTooling by Clang mutate.  The good, the bad, and the ugly.

# Class hierarchy and top-level organization

A C project is composed of *translation-unit*'s.  The children of a
translation unit are all *Decl*'s.  These *Decl*'s represent function
prototype, function declaration, typedef, global, and structures.
*Decl*'s may be composed of more *Decl*'s and/or of *Stmt*'s.  Every
`clang-mutate` AST is either a *Decl* or a *Stmt*.  The majority of a
typical C source file, i.e. the function bodies aside from local
variable declarations will be *Stmt*'s.

    translation-unit
    \-Decl (function, typedefs, globals, structures)
       \- Stmt (function bodies, maybe right hand sides of globals)

clang-mutate maintains a global table of translation units, defined in
TU.h.  Each translation unit (TU) manages the data for a single compiled
file, including:
* The clang CompilerInstance for the compilation.
* A table of clang-mutate ASTs.
* An optional BinaryAddressMap, when DWARF information is available.
* An optional LLVMInstructionMap, when LLVM information is available.
* Data about the variables in scope at each program point.
* JSON entries for auxiliary database information.

The AST table for a TU is constructed during a single traversal of a
clang translation unit; since some of clang's information about a
compilation is transient, all data that we want to keep around is
stored to the clang-mutate AST representation during this traversal.

Later, mutation operations are described relative to the clang-mutate
ASTs for a TU, and implemented using the clang rewrite buffers managed
by the translation unit's CompilerInstance.  These rewrite buffers
can be reset, allowing clang-mutate to generate multiple variants
from a single source file without re-parsing the original source.

# Locations

There are two types of locations.  *Source locations* and *defining
locations*.

# Full statements

A *full statement* is an AST whose parent is either a block statement
(class "CompoundStmt") or NoAst (for top-level declarations).

# Source ranges

clang-mutate's AST has two source ranges associated to it.  The
*source range* is the range in the original source file that covers
the text of the statement, not including any trailing semicolon. So
the source range for "x + y;" contains the text "x + y", and the
source range for "if (x) { y; }" is all of "if (x) { y; }".

The *normalized source range* will also include the trailing semicolon
for full statements, as appropriate.

## Source range asymmetry

When the user gets the text of a statement, the text for the source range
is always returned.  When a mutation targets a statement, the normalized
range is used for the target. That is, if AST #17 in TU 0 is the full
statement "x + y;", then "get 0 17" produces "x + y", but "set 0 17" will
overwrite all of "x + y;".

To compensate for this asymmetry, the mutations will default to adding
a semicolon to the mutation text when the mutation target is a full
statement and the text does not end with ';' or '}'.  This ensures that
the composition of get and set is idempotent (maybe modulo whitespace?).

# Conditional defines

Everything is per *this* compilation, the rewriting can not address
multiple paths on conditional defines.

# Structure Definitions

* Can easily map from the use of a structure to the *defining
    location*.
* Structures are represented and may be manipulated programmatically.
  Probably composed of *Decl*'s.
* Layout information?  Should it know? `offsetof`
* Composed of subclasses of Decl.

# Macros
* Botch function-like macros currently
* Can map macro to definition (defining location -- read: text).
* Just text, no ASTs, invisible to `clang-mutate`

# `#include`'s

Literally included, but clang tracks the file source for every AST.
LibTooling is careful to go through and ensure that it only operates
on the original file.

# Batch

Here "batch" means applying multiple transforms in a single run of the
tool.  This is desirable because currently much of our time is spent
repeatedly parsing headers.

* Not straightforward, mainly because of the way LibTooling handles
  arguments.

* Also, edits which hit the same pieces of the backing source buffer
  can cause faults in clang.

# Issues

## Fighting Clang assumptions

Clang and LibTooling seem to work very well, *if* what you want to do
is something that the Clang/LibTooling developers want you to do.

* Visit methods have a baked in traversal order, developers unwilling
  to consider other traversals.

* Rewrite buffers, despite a functional rope-based backing store,
  expose a mutable interface in which it is easy to crash Clang by
  attempting multiple modifications.

## Speed

Both the original C file and the headers must be parsed every time, so
e.g., a typical 100K-eval run of evo-deco would require parsing the
`#included` files ~300K times.  There is no built-in way to save an
AST representation of a program to then make subsequent modifications.

Workarounds

* Dig beneath LibTooling and maybe we could then access the ASTs and
  source buffers directly in such a way that we can (i) repeatedly
  modify an program and/or (ii) save a program representation to be
  reloaded by a later invocation without re-parsing everything.
* Use Clang only for the initial serialization of ASTs into a custom
  representation which includes all of the source information required
  for later serialization.  Then re-write all of the traversal and
  rewriting routines ourselves.
* Look into Clang support for pre-compiled headers.
* Consider checkpointing the `clang-mutate` process *after* it has
  parsed the headers but before any mutations have been applied, and
  then forking that process for every subsequent `clang-mutate`
  invocation.
