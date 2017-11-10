
#ifndef FIELD_DEF
#error "Must #declare the FIELD_DEF macro before #including AstFields.h"
#endif

#define AST_FIELD_P(Name, T, Descr, Predicate, Body) \
    FIELD_DEF(Name, T, Descr, Predicate, Body)

#define AST_FIELD(Name, T, Descr, Body) \
    FIELD_DEF(Name, T, Descr, true, Body)

AST_FIELD( counter, AstRef,
  "Unique identifier for AST node within a translation unit.",
  { return ast.counter(); }
  )

AST_FIELD( parent_counter, AstRef,
  "Unique identifier for parent node within a translation unit.",
  { return ast.parent(); }
  )

AST_FIELD( ast_class, std::string,
  "Class name of AST node.",
  { return ast.className(); }
  )

AST_FIELD( src_file_name, std::string,
  "Path to original source file.",
  { return ast.srcFilename(); }
  )

AST_FIELD( is_decl, bool,
  "Is this node a declaration?",
  { return ast.isDecl(); }
  )

AST_FIELD_P( declares, std::vector<std::string>,
  "A list of identifiers declared by this node.",
  !ast.declares().empty(),
  { return ast.declares(); }
  )

AST_FIELD( guard_stmt, bool,
  "Is this a guard (e.g. the predicate in an 'if' statement)?",
  { return ast.isGuard(); }
  )

AST_FIELD( full_stmt, bool,
  "Is this an immediate child of a compound statement?",
  { return ast.isFullStmt(); }
  )

AST_FIELD( syn_ctx, SyntacticContext,
  "What is the surrounding syntactic context of this statement?",
  { return ast.syntacticContext(); }
  )

AST_FIELD( children, std::vector<AstRef>,
  "The ordered list of immediate children of this node.",
  { return ast.children(); }
  )

AST_FIELD_P( stmt_list, std::vector<AstRef>,
  "For a CompoundStmt, the list of immediate children.",
  (ast.className() == "CompoundStmt"),
  { return ast.children(); }
  )

AST_FIELD_P( successors, std::vector<AstRef>,
  "The list of immediate CFG successors of this node.",
  (!ast.successors().empty()),
  { return ast.successors(); }
  )

AST_FIELD_P( opcode, std::string,
  "For a BinaryOp, the name of the operation.",
  (ast.opcode() != ""),
  { return ast.opcode(); }
  )

AST_FIELD( macros, std::set<Hash>,
  "The macros used by this statement.\n",
  { return ast.macros(); }
  )

AST_FIELD( includes, std::set<std::string>,
  "Header files that must be #included to use this statement.",
  { return ast.includes(); }
  )

AST_FIELD( types, std::vector<Hash>,
  "Types referenced by this statement.\n"
  "Types are given as hashes that can be looked up\n"
  "in the auxiliary type database. See _Type Database_.\n",
  { return ast.types(); }
  )

AST_FIELD_P( expr_type, Hash,
  "For an Expr, the type of the expression.\n"
  "Given as a hash that can be looked up\n"
  "in the auxiliary type database. See _Type Database_.\n",
  ( ast.expr_type() != 0 ),
  { return ast.expr_type(); }
  )

AST_FIELD( unbound_vals, std::set<VariableInfo>,
  "Free variables appearing in this node.",
  { return ast.freeVariables(); }
  )

AST_FIELD( unbound_funs, std::set<FunctionInfo>,
  "Free functions appearing in this node.",
  { return ast.freeFunctions(); }
  )

AST_FIELD( scopes, std::vector<std::vector<std::string> >,
  "Identifiers in scope at this node, defined at each enclosing scope.",
  { return tu.scopes.get_names_in_scope_from(ast.scopePosition(), 1000); }
  )

AST_FIELD( begin_src_line, unsigned int,
  "Source line of the start of this node.",
  { return ast.begin_src_pos().getLine(); }
  )

AST_FIELD( begin_src_col, unsigned int,
  "Source column of the start of this node.",
  { return ast.begin_src_pos().getColumn(); }
  )

AST_FIELD( end_src_line, unsigned int,
  "Source line of the end of this node.",
  { return ast.end_src_pos().getLine(); }
  )

AST_FIELD( end_src_col, unsigned int,
  "Source column of the end of this node.",
  { return ast.end_src_pos().getColumn(); }
  )

AST_FIELD( begin_off, unsigned int,
  "Source offset of the start of this node's text.",
  { return ast.initial_offset(); }
  )

AST_FIELD( end_off, unsigned int,
  "Source offset of the end of this node's text.",
  { return ast.final_offset(); }
  )

AST_FIELD( begin_norm_off, unsigned int,
  "Source offset of the start of this node's normalized text.",
  { return ast.initial_normalized_offset(); }
  )

AST_FIELD( end_norm_off, unsigned int,
  "Source offset of the start of this node's normalized text.",
  { return ast.final_normalized_offset(); }
  )

AST_FIELD( orig_text, std::string,
  "Original source code for this node.",
  {
      RewriterState state;
      getTextAs(ast.counter(), "$result", true)->run(state);
      return state.vars["$result"];
  } )

AST_FIELD( src_text, std::string,
  "Original source code, with free identifiers X replaced by (|X|).",
  {
      RewriterState state;
      getTextAs(ast.counter(), "$result")->run(state);
      return ast.replacements().apply_to(state.vars["$result"]);
  } )

AST_FIELD_P( binary_file_path, std::string,
  "Path to compiled binary.",
  ast.has_bytes(),
  { return tu.addrMap.getPath(); }
  )

AST_FIELD_P( begin_addr, unsigned long,
  "Initial address of the binary range for this statement.",
  ast.has_bytes(),
  {
      return ast.binaryAddressRange().value().first;
  } )

AST_FIELD_P( end_addr, unsigned long,
  "Final address of the binary range for this statement.",
  ast.has_bytes(),
  {
      return ast.binaryAddressRange().value().second;
  } )

AST_FIELD_P( binary_contents, std::string,
  "A hex string representation of the bytes associated to this statement.",
  ast.has_bytes(),
  {
      Bytes bytes = ast.bytes().value();
      std::stringstream ret;

      ret << std::hex << std::setfill('0');
      for ( Bytes::const_iterator byteIter = bytes.begin();
            byteIter != bytes.end();
            byteIter++ )
      {
        ret << std::setw(2) << static_cast<int>(*byteIter) << " ";
      }

      return ret.str().empty() ?
             ret.str() :
             ret.str().substr(0, ret.str().length() - 1);
  } )

AST_FIELD_P( llvm_ir, Instructions,
  "A list of LLVM instructions associated to this statement.",
  ast.has_llvm_ir(),
  {
      return ast.llvm_ir().value();
  } )

AST_FIELD_P( llvm_ir_file_path, std::string,
  "Path to compiled LLVM IR.",
  ast.has_llvm_ir(),
  { return tu.llvmInstrMap.getPath(); }
  )


AST_FIELD_P( base_type, std::string,
  { "For field declarations, the base type name." },
  ast.is_field_decl(),
  { return ast.base_type(); }
  )

AST_FIELD_P( bit_field_width, unsigned,
  "For bitfield declarations, the field width.",
  ast.is_field_decl() && ast.is_bit_field(),
  { return ast.bit_field_width(); }
  )

AST_FIELD_P( array_length, unsigned long,
  "For array-type field declarations, the array length.",
  ast.is_field_decl() && ast.array_length() > 1,
  { return ast.array_length(); }
  )

AST_FIELD( in_macro_expansion, bool,
  "Is this part of a macro expansion?",
  { return ast.inMacroExpansion(); }
  )

AST_FIELD_P( annotations, std::vector<std::string>,
	   "Annotations of a declaration. (Found from __attribute(( annotate(...) ))).",
	     !ast.annotations().empty();,
	   { return ast.annotations(); }
	   )

AST_FIELD_P(accessed_label_name, std::string,
	    "Label name of a struct member access.",
	    ast.isStmt() && ast.isMemberExpr(),
	    { return ast.label_name(); }
	   )


#undef FIELD_DEF
