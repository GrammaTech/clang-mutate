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

# Locations

There are two types of locations.  *Source locations* and *defining
locations*.

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
