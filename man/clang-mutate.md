% CLANG_MUTATE(1) Clang Source-to-Source Translation
% GrammaTech Inc.
% December 16, 2015

# NAME

clang-mutate - Clang mutation tool

# SYNOPSIS

clang-mutate [*options*] <*source0*> [...<*sourceN*>]...

# DESCRIPTION

TODO

# OPTIONS

TODO

# Examples

TODO

# Source Modification

`clang-mutate` is a tool for source modification.  In most cases
arbitrary changes to program source code results in non-compilable
source.  In some cases `clang-mutate` will automatically take the
required action to ensure that modifications result in sensible
source, however in general `clang-mutate` leaves all non-trivial
decisions to the users.  In these cases `clang-mutate` will instead
provides the information required to make those changes.  This section
enumerates automated and manual rewriting decisions.

## Automated rewriting decisions

semicolons
:   In general full statements (within a `clang::CompoundStmt`)
    require a terminating semi-colon, while regular statements (e.g.,
    inside of a `clang::Stmt`) do not need such termination.
    `clang-mutate` will automatically add semicolons to full
    statements.

## Manual rewriting decisions

free variables
:   Often a snippet of source code which will be inserted into a
    program will include free variables which must be bound to in
    scope variables at the point of insertion.  Although clang does
    not replace free variables automatically, it does list in-scope
    variables via the *get-scope* functionality.  It also replaces
    free variables in json output with easily search/replaced place
    holders.

# Terminology

full statement
:   A `clang::Stmt` whose parent is a `clang::CompoundStmt`.  These
    correspond to top-level statements which should be terminated in
    semicolons.
