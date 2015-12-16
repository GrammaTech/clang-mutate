% CLANG_MUTATE(1) Clang Source-to-Source Translation
% GrammaTech Inc.
% December 16, 2015

# NAME

clang-mutate - Clang Source to Source translation tool

# SYNOPSIS

clang-mutate [*options*] <*source0*> [...<*sourceN*>]...

# DESCRIPTION

TODO

# OPTIONS

TODO

# Examples

TODO

# Discussion

`clang-mutate` is a tool for source modification.  In most cases
arbitrary changes to program source code results in non-compilable
source.  In some cases `clang-mutate` will automatically take the
required action to ensure that modifications result in sensible
source, however in general `clang-mutate` leaves all non-trivial
decisions to the users.  We discuss some specific cases and design
decisions below.

## Insertion of Semicolons

In general full statements (within a `clang::CompoundStmt`) require a
terminating semi-colon, while regular statements (e.g., inside of a
`clang::Stmt`) do not need such termination.  When performing AST
commands (e.g., `insert`, `swap`) `clang-mutate` will automatically
add semicolons to full statements, however when performing value
commands (e.g., `insert-value`, `set`) `clang-mutate` will not
automatically insert semicolon.  This allows the insertion of other
objects such as blocks, or comments which do not require semicolon
termination.

## Free variable renaming

Often a snippet of source code which will be inserted into a program
will include free variables which must be bound to in scope variables
at the point of insertion.  Although clang does not replace free
variables automatically, it does list in-scope variables via the
*get-scope* functionality.  It also replaces free variables in json
output with easily search/replaced place holders.

# Terminology

full statement
:   A `clang::Stmt` whose parent is a `clang::CompoundStmt`.  These
    correspond to top-level statements which should be terminated in
    semicolons.  These are labeled *full_stmt* in json output.

guard statement
:   A `clang::Stmt` which exists in a `if` or `for` conditional
    guard.  These are labeled *guard_stmt* in json output.
