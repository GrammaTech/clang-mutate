% CLANG_MUTATE(1) Clang Source-to-Source Translation
% GrammaTech Inc.
% December 16, 2015

# NAME

clang-mutate - Clang Source to Source translation tool

# SYNOPSIS

clang-mutate [*options*] <*source0*> [...<*sourceN*>]...

# DESCRIPTION

`clang-mutate` performs a number of local source-to-source
transformations (or mutations) over C language source.

This tool uses the Clang Tooling infrastructure, see
http://clang.llvm.org/docs/LibTooling.html for details.

# OPTIONS

-help
:   Print usage information.

-stmt1
:   Statement id for mutation operations.

-stmt2
:   Second statement id for mutation operations.

-value1
:   String value for mutation operations.

-value2
:   Second string value for mutation operations.

-file1
:   File holding the value1 to use for mutation operations.

-file2
:   File holding the value2 to use for mutation operations.

-binary
:   Binary with DWARF information for line-to-address mapping.

# MUTATION OPERATIONS

-cut
:   Cut stmt1 from the source.

-insert
:   Copy stmt1 into the source before stmt2.

-insert-value
:   Insert value1 before stmt1.

-swap
:   Swap stmt1 and stmt2 in the source.

-set
:   Set the text of stmt1 to value1.

-set2
:   Set the text stmt1 to value1 and the text of stmt2 to value2.

-set-range
:   Set the range of statements from stmt1 to stmt2 to value1.

-set-func
:   Replace the entire declaration of the function containing stmt1
    to value1.
    
# INSPECTION OPERATIONS

-number
:   Annotate the source marking each statement with its ID.

-ids
:   Print a count of the number of statements in the source.

-annotate
:   Annotate the source marking each statement with its ID and class.

-get
:   Print the text of stmt1.

-get-scope
:   Get the first n variables in scope at stmt1.

-list
:   List every statement's id, class, and range.

-json
:   List JSON-encoded descriptions for every statement.

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
