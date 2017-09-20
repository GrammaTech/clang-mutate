% CLANG_MUTATE(1) Clang Source-to-Source Translation
% GrammaTech Inc.
% December 16, 2015

# NAME

clang-mutate - Clang Source to Source translation tool

# SYNOPSIS

clang-mutate [*options*] <*source0*> [...<*sourceN*>] [-- [*compiler-flags*]]

# DESCRIPTION

`clang-mutate` performs a number of local source-to-source
transformations (or mutations) over C language source. `clang-mutate`
may also be used to print information of program source in JSON or
S-expression format.

This tool uses the Clang Tooling infrastructure, see
http://clang.llvm.org/docs/LibTooling.html for details.

By default, `clang-mutate` searches for a compile commands database
in the current directory and its parent directories (as specified in
the Clang Tooling documentation) unless `--` is included in the command
line options. If `--` is used, clang compiler flags may optionally be
specified after the `--`.

# OPTIONS

-help, -help-list, -help-hidden, -help-list-hidden
:   Print usage information.

--
:   Do not attempt to use a compile commands database.

-binary
:   Binary with DWARF information for line-to-address mapping.

-ctrl
:   Print a control character after output in interactive mode.

-dwarf-filepath-mapping
:   Mapping of filepaths used in compilation to new filepath.

-interactive
:   Run in interactive mode.

-p *DIR*
:   Build path used to read a compile commands database.

-print-all-options
:   Print all option values after command-line parsing.

-print-options
:   Print all non-default options after command-line parsing.

-llvm_ir
:   LLVM IR with debug information for line-to-instruction mapping.

-silent
:   Do not print prompts in interactive mode.

-version
:   Print version information.

## Mutation operands

-stmt1 *ID*
:   Statement *ID* for mutation operations.

-stmt2 *ID*
:   Second statement *ID* for mutation operations.

-value1 *VALUE*
:   String *VALUE* for mutation operations.

-value2 *VALUE*
:   Second string *VALUE* for mutation operations.

-file1 *FILE*
:   *FILE* holds the value1 to use for mutation operations.

-file2 *FILE*
:   *FILE* holds the value2 to use for mutation operations.

## Mutation operations

-cut
:   Cut stmt1 from the source.

-insert
:   Copy stmt1 into the source before stmt2.

-insert-value
:   Insert value1 before stmt1.

-set
:   Set the text of stmt1 to value1.

-set2
:   Set the text stmt1 to value1 and the text of stmt2 to value2.

-set-func
:   Replace the entire declaration of the function containing stmt1
    to value1.

-set-range
:   Set the range of statements from stmt1 to stmt2 to value1.

-swap
:   Swap stmt1 and stmt2 in the source.

## Inspection operations

-annotate
:   Annotate the source, marking each statement with its ID and class.

-aux=[*FLAGS*]
:   Include auxiliary information in JSON or S-expression output according
    to *FLAGS*. *FLAGS* should be a comma delimited list one or more of the
    following keywords.

    types
    :   Output a type database as described in section _TYPE
        DATABASE_.

    decls
    :   Include declarations in JSON or S-expression output.

    asts
    :   Include auxiliary AST fields in JSON or S-expression output.

-cfg
:   Include successors field with control-flow information in ASTs.

-fields=[*FIELDS*]
:   The fields flag may be used to control the field names calculated
    and returned by `clang-mutate`. *FIELDS* should be a comma-delimited
    list of field names.  See the _AST FIELDS_ section for
    names of the available fields and their contents.

-ids
:   Print a count of the number of statements in the source.

-list
:   List every statement's id, class, and range.

-number
:   Annotate the source, marking each statement with its ID.

-number-full
:   Annotate the source, marking each full statement with its ID.

-json
:   List JSON-encoded descriptions for every statement.

-sexp
:   List S-expression encoded descriptions for every statement.

# AST FIELDS

INCLUDE_FIELDS_MD

# TYPE DATABASE

When the `types` auxiliary flag is passed, `clang-mutate` will print a
JSON or S-expression object for each type encountered in the program.
Each type object has the following keys which provide type information
and which may be used to find the types referenced from the `type` field
for program elements.

array: string
:   A string representing the clang `ArraySizeModifier` associated
    with the type. This may hold one of the following values.

    `[]`
    :   Indicating a normal array, e.g., `X[4]`.

    `[static]`
    :   Indicating a static array, e.g., `X[static 4]`.

    `[*]`
    :   Indicating a star sized array, e.g., `X[*]`.

    `""`
    :   Indicating the type is not an array.

col
:   The column on which the type definition is located (if known).

decl
:   A string holding the code used to declare the type.

file
:   The file in which the type definition is located (if known).

hash
:   A hash field which corresponds to the `type` field on program
    element objects. See _AST FIELDS_.

i_file
:   A list of file names for `#include` directives leading to the
    definition of the type (if known).

line
:   The line in which the type definition is located (if known).

pointer
:   A boolean indicating if the type is a pointer type or not.

reqs
:   A list of hash values for types upon which the type declaration
    depends.

type
:   The name of the type.

# DISCUSSION

`clang-mutate` is a tool for source modification. In most cases
arbitrary changes to program source code result in non-compilable
source. In some cases `clang-mutate` will automatically take the
required action to ensure that modifications result in sensible
source, however in general `clang-mutate` leaves all non-trivial
decisions to the users. We discuss some specific cases and design
decisions below.

## Insertion of semicolons

In general full statements require a terminating semicolon, while
regular statements do not need such termination. When performing AST
commands (e.g., `insert`, `swap`) `clang-mutate` will automatically
add semicolons to full statements; however when performing value
commands (e.g., `insert-value`, `set`) `clang-mutate` will not
automatically insert semicolons. This allows the insertion of other
objects such as blocks or comments which do not require semicolon
termination.

## Free variable renaming

Often a snippet of source code which will be inserted into a program
will include free variables which must be bound to in scope variables
at the point of insertion. Although clang does not replace free
variables automatically, it does list in-scope variables in the
*scopes* field. It also replaces free variables in JSON or
S-expression output with placeholders that are easy to search/replace.

# EXAMPLES

    $ cat etc/hello.c
    #include <stdio.h>
    int main(int argc, char *argv[])
    {
      puts("hello");
      return 0;
    }

    $ clang-mutate -ids etc/hello.c --
    12

    $ clang-mutate -number etc/hello.c --
    #include <stdio.h>
    /* 0.1[ */int main(/* 0.2[ */int argc,/* ]0.2 */ /* 0.3[ */char *argv[]/* ]0.3 */)
    /* 0.4[ */{
      /* 0.5[ *//* 0.6[ *//* 0.7[ */puts/* ]0.7 *//* ]0.6 */(/* 0.8[ *//* 0.9[ *//* 0.10[ */"hello"/* ]0.10 *//* ]0.9 *//* ]0.8 */);/* ]0.5 */
      /* 0.11[ */return /* 0.12[ */0/* ]0.12 */;/* ]0.11 */
    }/* ]0.4 */
    /* ]0.1 */

    $ clang-mutate -cut -stmt1=11 etc/hello.c --
    #include <stdio.h>
    int main(int argc, char *argv[])
    {
      puts("hello");

    }

    $ clang-mutate -set -stmt1=9 -value1='"good bye"' etc/hello.c --
    #include <stdio.h>
    int main(int argc, char *argv[])
    {
      puts("good bye");
      return 0;
    }

# TERMINOLOGY

full statement
:   A `clang::Stmt` whose parent is a block statement (`clang::CompoundStmt`),
    could be made to be a block statement (for single statement if/loop
    bodies), or NoAst (for top-level declarations). These are labeled *full-stmt*
    in JSON and S-expression output.

guard statement
:   A `clang::Stmt` which exists in a `if` or `for` conditional
    guard. These are labeled *guard_stmt* in JSON and S-expression output.
