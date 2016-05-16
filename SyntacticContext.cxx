
CONTEXT( Generic,
         no_change,
         no_change,
         no_change )

CONTEXT( FullStmt,
         add_semicolon_if_unbraced,
         add_semicolon_if_unbraced,
         add_semicolon_if_unbraced )

CONTEXT( ListElt,
         trailing_comma,
         trailing_comma,
         trailing_comma )

CONTEXT( FinalListElt,
         trailing_comma,
         no_change,
         leading_comma )

CONTEXT( Braced,
         no_change,
         add_braces,
         no_change )

CONTEXT( Field,
         add_semicolon,
         add_semicolon,
         add_semicolon )

// TODO: This isn't exactly correct; for example,
//       struct declarations in C++ may end with
//       a brace, but must still be followed with
//       a semicolon.
CONTEXT( TopLevel,
         add_semicolon_if_unbraced,
         add_semicolon_if_unbraced,
         add_semicolon_if_unbraced )
         
#undef CONTEXT
