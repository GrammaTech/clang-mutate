# clang-mutate: Manipulate C-family ASTs with Clang

This tool provides an interface for performing operations on C and C++
source files.  Using the
[clang LibTooling interface](http://clang.llvm.org/docs/LibTooling.html),
it obtains an abstract syntax tree (AST) representation of the source
and provides enables the inspection or alteration the source ASTs.


## Inspection operations

`clang-mutate` can output the ASTs for input source file(s) formatted
either as JSON or S-expressions.  Output ASTs provide source
information to enable subsequent source rewriting by external tools.
Alternatively, it can be launched in interactive mode, and ASTs can be
explored in a plain text format more similar to source code.

Some of the inspection operations are:

`-annotate`
:   Display the source, annotating statements with their IDs
    and AST classes

`-ids`
:   Display the number of statements in the source

`-list`
:   List every statement's ID, AST class, and range

`-number`
:   Display the source, annotating statements with their IDs

`-number-full`
:   Display the source, annotating full statements with their IDs

`-cfg`
:   Include control-flow information in ASTs

Refer to the [manual](man/clang-mutate.template.md) for complete
documentation of available commands.


## Mutation operands

In `clang-mutate`, the mutation operands and operators are specified
as separate flags.  The `stmt1` and `stmt2` operands are used to
specify a numeric AST ID to which an operator will be applied. The
`value1` and `value2` operands are strings, and the `file1` and
`file2` operands may be used to specify a file whose contents will be
used as the strings for `value1` and `value2`, repectively.


## Mutation operations

Some of the mutation operations that are available include:

`-cut`
:   Cut `stmt1` from the source

`-insert`
:   Insert `value1` before `stmt1`

`-set`
:   Set the text of `stmt1` to `value1`

`-swap`
:   Swap `stmt1` and `stmt2` in the source

Refer to the [manual](man/clang-mutate.template.md) for complete
documentation of available mutation commands.


## Installation

As mentioned above, `clang-mutate` uses the clang LibTooling
interface, which means that the `clang-mutate` executable works best
when run from the same directory as clang. Run "make install" to build
and install.

A PKGBUILD file is provided for installation on Arch Linux systems.

`clang-mutate` has only been tested on Linux, although we don't know
of any reason it should not work in other OSes.  If you encounter
issues running in a different OS, please let us know by filing an
issue report or a merge request.  We're happy to incorporate changes
that will enable more general execution.


## Examples

A running example using the file `etc/hello.c`. Here is the source:

    $ cat etc/hello.c
    #include <stdio.h>
    int main(int argc, char *argv[])
    {
      puts("hello");
      return 0;
    }

Count the number of statements:

    $ clang-mutate -ids etc/hello.c --
    12

Print the source, annotated with statement IDs:

    $ clang-mutate -number etc/hello.c --
    #include <stdio.h>
    /* 0.1[ */int main(/* 0.2[ */int argc,/* ]0.2 */ /* 0.3[ */char *argv[]/* ]0.3 */)
    /* 0.4[ */{
      /* 0.5[ *//* 0.6[ *//* 0.7[ */puts/* ]0.7 *//* ]0.6 */(/* 0.8[ *//* 0.9[ *//* 0.10[ */"hello"/* ]0.10 *//* ]0.9 *//* ]0.8 */);/* ]0.5 */
      /* 0.11[ */return /* 0.12[ */0/* ]0.12 */;/* ]0.11 */
    }/* ]0.4 */
    /* ]0.1 */

Cut the return statement:

    $ clang-mutate -cut -stmt1=11 etc/hello.c --
    #include <stdio.h>
    int main(int argc, char *argv[])
    {
      puts("hello");

    }

Replace the string "hello" with the string "good bye":

    $ clang-mutate -set -stmt1=9 -value1='"good bye"' etc/hello.c --
    #include <stdio.h>
    int main(int argc, char *argv[])
    {
      puts("good bye");
      return 0;
    }

Use the `tools/perforate-loops` script to halve the number of loop iterations by
replacing `++` or `--` with `+=2` or `-=2`, respectively:

    $ make etc/loop
    cc     etc/loop.c   -o etc/loop
    $ ./etc/loop
    hello 0 10
    hello 1 9
    hello 2 8
    hello 3 7
    hello 4 6
    hello 5 5
    hello 6 4
    hello 7 3
    hello 8 2
    hello 9 1

    $ ./tools/perforate-loops etc/loop.c > /tmp/faster.c
    $ make /tmp/faster
    cc     /tmp/faster.c   -o /tmp/faster
    $ /tmp/faster
    hello 0 10
    hello 2 8
    hello 4 6
    hello 6 4
    hello 8 2

Use the `tools/strings-to` script to replace string literals:

    $ ./tools/strings-to etc/hello.c foo
    #include <stdio.h>
    int main(int argc, char *argv[])
    {
      puts("foo");
      return 0;
    }
