/*
 * RtedTransf_variable.cpp
 *
 *  Created on: Jul 7, 2009
 *      Author: panas2
 */


#include <rose.h>
#include <string>
#include "RtedSymbols.h"
#include "DataStructures.h"
#include "RtedTransformation.h"

using namespace std;
using namespace SageInterface;
using namespace SageBuilder;


// ------------------------ VARIABLE SPECIFIC CODE --------------------------

bool RtedTransformation::isVarInCreatedVariables(SgInitializedName* n) {
  bool ret=false;
  ROSE_ASSERT(n);
  Rose_STL_Container<SgInitializedName*>::const_iterator it=variable_declarations.begin();
  for (;it!=variable_declarations.end();++it) {
    SgInitializedName* initName = *it;
    if (initName==n) {
      ret=true;
      break;
    }
  }
  return ret;
}

void RtedTransformation::visit_isSgVariableDeclaration(SgNode* n) {
  SgVariableDeclaration* varDecl = isSgVariableDeclaration(n);
  Rose_STL_Container<SgInitializedName*> vars = varDecl->get_variables();
  Rose_STL_Container<SgInitializedName*>::const_iterator it = vars.begin();
  cerr << " ...... CHECKING Variable Declaration " << endl;
  for (;it!=vars.end();++it) {
    SgInitializedName* initName = *it;
    ROSE_ASSERT(initName);
    // need to get the type and the possible value that it is initialized with
    cerr << "      Detected initName : " << initName->unparseToString() ;
    cerr <<"  type : " << initName->get_type()->unparseToString() << endl;
    variable_declarations.push_back(initName);
  }
}


void RtedTransformation::insertVariableCreateCall(SgInitializedName* initName
						  ) {
  SgStatement* stmt = getSurroundingStatement(initName);
  // make sure there is no extern in front of stmt
  bool externQual = isGlobalExternVariable(stmt);
  if (externQual) {
    cerr << "Skipping this insertVariableCreateCall because it probably occurs multiple times (with and without extern)." << endl;
    return;
  }


  if (isSgStatement(stmt)) {
    SgScopeStatement* scope = stmt->get_scope();
    string name = initName->get_mangled_name().str();

    ROSE_ASSERT(scope);
    // what if there is an array creation within a ClassDefinition
    if ( isSgClassDefinition(scope)) {
      // new stmt = the classdef scope
      SgClassDeclaration* decl = isSgClassDeclaration(scope->get_parent());
      ROSE_ASSERT(decl);
      stmt = isSgVariableDeclaration(decl->get_parent());
      if (!stmt) {
	cerr << " Error . stmt is unknown : " << decl->get_parent()->class_name() << endl;
	exit(1);
      }
      scope = scope->get_scope();
      // We want to insert the stmt before this classdefinition, if its still in a valid block
      cerr <<" ....... Found ClassDefinition Scope. New Scope is : " << scope->class_name() << "  stmt:" << stmt->class_name() <<endl;
    }
    // what is there is an array creation in a global scope
    else if (isSgGlobal(scope)) {
      cerr <<"RuntimeInstrumentation :: WARNING - Scope not handled!!! : " << name << " : " << scope->class_name() << endl;
      // We need to add this new statement to the beginning of main
      // get the first statement in main as stmt
      stmt = mainFirst;
      scope = stmt->get_scope();

      //SgExprStatement* exprStmt = buildVariableCreateCallStmt( initName, stmt);
      // kind of hackey.  Really we should be prepending into main's body, not
      // inserting relative its first statement.
      //insertStatementBefore(isSgStatement(stmt), exprStmt);
    }
    if (isSgBasicBlock(scope)) {
      // insert new stmt (exprStmt) after (old) stmt
      SgExprStatement* exprStmt = buildVariableCreateCallStmt( initName, stmt);
#if 1
      cerr << "++++++++++++ stmt :"<<stmt << " mainFirst:"<<mainFirst<<
      "   initName->get_scope():"<<initName->get_scope() <<
      "   mainFirst->get_scope():"<<mainFirst->get_scope()<<endl;
      if( stmt == mainFirst && initName->get_scope()!=mainFirst->get_scope()) {
        insertStatementBefore(isSgStatement(stmt), exprStmt);
        cerr << "+++++++ insert Before... "<<endl;
      } else {
        // insert new stmt (exprStmt) after (old) stmt
        insertStatementAfter(isSgStatement(stmt), exprStmt);
        cerr << "+++++++ insert After... "<<endl;
      }
#endif
    }
    else if (isSgNamespaceDefinitionStatement(scope)) {
      cerr <<"RuntimeInstrumentation :: WARNING - Scope not handled!!! : " << name << " : " << scope->class_name() << endl;
    } else {
      cerr
	<< "RuntimeInstrumentation :: Surrounding Block is not Block! : "
	<< name << " : " << scope->class_name() << endl;
      ROSE_ASSERT(false);
    }
  } else {
    cerr
      << "RuntimeInstrumentation :: Surrounding Statement could not be found! "
      << stmt->class_name() << endl;
    ROSE_ASSERT(false);
  }


}


SgExprStatement*
RtedTransformation::buildVariableCreateCallStmt( SgInitializedName* initName, SgStatement* stmt, bool forceinit) {
    string name = initName->get_mangled_name().str();
    SgScopeStatement* scope = stmt->get_scope();


    ROSE_ASSERT( initName);
    ROSE_ASSERT( stmt);
    ROSE_ASSERT( scope);

    // build the function call : runtimeSystem-->createArray(params); ---------------------------
    SgExprListExp* arg_list = buildExprListExp();
    SgExpression* callName = buildString(initName->get_name().str());
    //SgExpression* callName = buildStringVal(initName->get_name().str());
    SgExpression* callNameExp = buildString(name);
    SgInitializer* initializer = initName->get_initializer();
    SgExpression* fileOpen = buildString("no");
    bool initb = false;
    if (initializer) initb=true;
    SgExpression* initBool = buildIntVal(0);
    if (initb || forceinit)
      initBool = buildIntVal(1);

    SgExpression* var_ref = buildVarRefExp( initName, scope);
    // TODO 2: Remove this if statement once the RTS handles user types
    // Note: This if statement is a hack put in place to pass the array out of
    // bounds tests
    if( isSgVariableDeclaration( stmt)) {
      SgInitializedName* first_var_name
        = isSgVariableDeclaration( stmt)->get_variables()[0];

      if(
          isSgClassDeclaration(
              isSgVariableDeclaration(stmt)->get_baseTypeDefiningDeclaration())
            && first_var_name != initName
      ) {
        var_ref = new SgDotExp(
          // file info
          initName->get_file_info(),
          // lhs
          buildVarRefExp( first_var_name, scope),
          // rhs
          var_ref,
          // type
          initName->get_type()
        );
      }
    }


    appendExpression(arg_list, callName);
    appendExpression(arg_list, callNameExp);
    //appendExpression(arg_list, typeName);
    appendAddressAndSize(initName, isSgVarRefExp(var_ref), stmt, arg_list,1);


    appendExpression(arg_list, initBool);
    appendExpression(arg_list, fileOpen);

	// TODO djh 1: append classname if basetype is SgClassType as well
	// 		see appendAddressAndSize, which handles the basetype stuff
    SgClassType* sgClass = isSgClassType(initName->get_type());
    if (sgClass) {
        appendExpression(arg_list, buildString(
			isSgClassDeclaration( sgClass -> get_declaration() ) 
				-> get_mangled_name() )
		);
    } else {
        appendExpression(arg_list, buildString(""));
    }

    SgExpression* filename = buildString(stmt->get_file_info()->get_filename());
    SgExpression* linenr = buildString(RoseBin_support::ToString(stmt->get_file_info()->get_line()));
    appendExpression(arg_list, filename);
    appendExpression(arg_list, linenr);

    SgExpression* linenrTransformed = buildString("x%%x");
    appendExpression(arg_list, linenrTransformed);

    ROSE_ASSERT(roseCreateVariable);
    string symbolName2 = roseCreateVariable->get_name().str();
    SgFunctionRefExp* memRef_r = buildFunctionRefExp(  roseCreateVariable);
    SgFunctionCallExp* funcCallExp = buildFunctionCallExp(memRef_r,
                arg_list);
    SgExprStatement* exprStmt = buildExprStatement(funcCallExp);
    string empty_comment = "";
    attachComment(exprStmt,empty_comment,PreprocessingInfo::before);
    string comment = "RS : Create Variable, paramaters : (name, mangl_name, type, basetype, address, sizeof, initialized, fileOpen, classname, filename, linenr, linenrTransformed)";
    attachComment(exprStmt,comment,PreprocessingInfo::before);

    return exprStmt;
}


void RtedTransformation::appendAddressAndSize(SgInitializedName* initName,
					      SgExpression* varRefE,
					      SgStatement* stmt,
					      SgExprListExp* arg_list,
					      int appendType
					      ) {

    SgScopeStatement* scope = NULL;
    SgType* basetype = NULL;

    if( initName) {
        SgType* type = initName->get_type();
        ROSE_ASSERT(type);
        SgExpression* basetypeStr = buildString("");
        if (isSgPointerType(type)) {
        //	typestr="pointer";
        basetype = isSgPointerType(type)->get_base_type();
        } else if( isSgArrayType( type )) {
			basetype = isSgArrayType( type )->get_base_type();
		}
        if (basetype)
          basetypeStr = buildString(basetype->class_name());
        SgExpression* ctypeStr = buildString(type->class_name());
        if (appendType==1) {
            appendExpression(arg_list, ctypeStr);
            appendExpression(arg_list, basetypeStr);
        }

        scope = initName->get_scope();
    }

    SgExpression* exp = varRefE;
    if (    isSgClassType(basetype) ||
            isSgTypedefType(basetype) ||
            isSgClassDefinition(scope)) {

        // member -> &( var.member )
        exp = getUppermostLvalue( varRefE );
    }

    //int derefCounter=0;//getTypeOfPointer(initName);
    //SgExpression* fillExp = getExprBelowAssignment(varRefE, derefCounter);
    //ROSE_ASSERT(fillExp);

    appendExpression(
        arg_list,
        buildCastExp(
            buildAddressOfOp( exp ),
            buildUnsignedLongLongType()
        )
    );

    // consider, e.g.
    //
    //      char* s1;
    //      char s2[20];
    //
    //      s1 = s2 + 3;
    //      
    //  we only want to access s2..s2+sizeof(char), not s2..s2+sizeof(s2)
    //
    //  but we do want to create the array as s2..s2+sizeof(s2)     
    if( appendType & 2 && isSgArrayType( varRefE->get_type() )) {
        appendExpression(
            arg_list,
            buildSizeOfOp( isSgArrayType( varRefE->get_type() )->get_base_type())
        );
    } else {
        appendExpression(
            arg_list,
            buildSizeOfOp( exp )
        );
    }
}


void RtedTransformation::insertInitializeVariable(SgInitializedName* initName,
						  SgVarRefExp* varRefE,
						  bool ismalloc
						  ) {
	  SgStatement* stmt=NULL;
	if (varRefE->get_parent()) // we created a verRef for AssignInitializers which do not have a parent
		stmt = getSurroundingStatement(varRefE);
	else
		stmt = getSurroundingStatement(initName);

  // make sure there is no extern in front of stmt

  if (isSgStatement(stmt)) {
    SgScopeStatement* scope = stmt->get_scope();
    string name = initName->get_mangled_name().str();
    cerr << "          ... running insertInitializeVariable :  "<<name<< "   scope: " << scope->class_name() << endl;

    ROSE_ASSERT(scope);
    // what if there is an array creation within a ClassDefinition
    if ( isSgClassDefinition(scope)) {
      // new stmt = the classdef scope
      SgClassDeclaration* decl = isSgClassDeclaration(scope->get_parent());
      ROSE_ASSERT(decl);
      stmt = isSgVariableDeclaration(decl->get_parent());
      if (!stmt) {
	cerr << " Error . stmt is unknown : " << decl->get_parent()->class_name() << endl;
	exit(1);
      }
      scope = scope->get_scope();
      // We want to insert the stmt before this classdefinition, if its still in a valid block
      cerr <<" ------->....... Found ClassDefinition Scope. New Scope is : " << scope->class_name() << "  stmt:" << stmt->class_name() <<"\n\n\n\n"<<endl;
    }
    // what is there is an array creation in a global scope
    else if (isSgGlobal(scope)) {
      cerr <<" ------->....... RuntimeInstrumentation :: WARNING - Scope not handled!!! : " << name << " : " << scope->class_name() << "\n\n\n\n" <<endl;
      // We need to add this new statement to the beginning of main
      // get the first statement in main as stmt
      stmt = mainFirst;
      scope=stmt->get_scope();
    }
    if (isSgBasicBlock(scope) ||
	isSgIfStmt(scope) ||
	isSgForStatement(scope)) {
      // build the function call : runtimeSystem-->createArray(params); ---------------------------
      SgExprListExp* arg_list = buildExprListExp();
      SgExpression* simplename = buildString(initName->get_name().str());
      appendExpression(arg_list, simplename);
      SgExpression* callName = buildString(initName->get_mangled_name().str());
      appendExpression(arg_list, callName);

      appendAddressAndSize(initName, varRefE, stmt, arg_list,1);


      SgIntVal* ismallocV = buildIntVal(0);
      if (ismalloc)
        ismallocV = buildIntVal(1);
      appendExpression(arg_list, ismallocV);

      SgExpression* filename = buildString(stmt->get_file_info()->get_filename());
      SgExpression* linenr = buildString(RoseBin_support::ToString(stmt->get_file_info()->get_line()));
      appendExpression(arg_list, filename);
      appendExpression(arg_list, linenr);
      SgExpression* linenrTransformed = buildString("x%%x");
      appendExpression(arg_list, linenrTransformed);

      appendExpression(arg_list, buildString(removeSpecialChar(stmt->unparseToString())));

      ROSE_ASSERT(roseInitVariable);
      string symbolName2 = roseInitVariable->get_name().str();
      SgFunctionRefExp* memRef_r = buildFunctionRefExp(  roseInitVariable);
      SgFunctionCallExp* funcCallExp = buildFunctionCallExp(memRef_r,
                  arg_list);

      SgExprStatement* exprStmt = buildExprStatement(funcCallExp);
      string empty_comment = "";
      attachComment(exprStmt,empty_comment,PreprocessingInfo::before);
      string comment = "RS : Init Variable, paramaters : (name, mangl_name, tpye, basetype, address, size, ismalloc, filename, line, linenrTransformed, error line)";
      attachComment(exprStmt,comment,PreprocessingInfo::before);

      // insert new stmt (exprStmt) before (old) stmt
      insertStatementAfter(isSgStatement(stmt), exprStmt);
    } // basic block
    else if (isSgNamespaceDefinitionStatement(scope)) {
      cerr <<" ------------> RuntimeInstrumentation :: WARNING - Scope not handled!!! : " << name << " : " << scope->class_name() << "\n\n\n\n"<<endl;
    } else {
      cerr
	<< " -----------> RuntimeInstrumentation :: Surrounding Block is not Block! : "
	<< name << " : " << scope->class_name() << "  - " << stmt->unparseToString() << endl;
      ROSE_ASSERT(false);
    }
  } else {
    cerr
      << "RuntimeInstrumentation :: Surrounding Statement could not be found! "
      << stmt->class_name() << "  " << stmt->unparseToString() << endl;
    ROSE_ASSERT(false);
  }


}



void RtedTransformation::insertAccessVariable(SgVarRefExp* varRefE,
			SgPointerDerefExp* derefExp
						  ) {
	SgStatement* stmt = getSurroundingStatement(varRefE);
  // make sure there is no extern in front of stmt
  SgInitializedName* initName = varRefE->get_symbol()->get_declaration();

  SgDotExp* parent_dot = isSgDotExp( varRefE -> get_parent() );
  if(	parent_dot
		&& parent_dot -> get_lhs_operand() == varRefE ) {
	  //	x = s.y
	  // does not need a var ref to y, only to s
	  return;
  }

  if (isSgStatement(stmt)) {
    SgScopeStatement* scope = stmt->get_scope();
    string name = initName->get_mangled_name().str();
    cerr << "          ... running insertAccessVariable :  "<<name<< "   scope: " << scope->class_name() << endl;

    ROSE_ASSERT(scope);
    // what if there is an array creation within a ClassDefinition
    if ( isSgClassDefinition(scope)) {
      // new stmt = the classdef scope
      SgClassDeclaration* decl = isSgClassDeclaration(scope->get_parent());
      ROSE_ASSERT(decl);
      stmt = isSgVariableDeclaration(decl->get_parent());
      if (!stmt) {
	cerr << " Error . stmt is unknown : " << decl->get_parent()->class_name() << endl;
	exit(1);
      }
      scope = scope->get_scope();
      // We want to insert the stmt before this classdefinition, if its still in a valid block
      cerr <<" ------->....... Found ClassDefinition Scope. New Scope is : " << scope->class_name() << "  stmt:" << stmt->class_name() <<"\n\n\n\n"<<endl;
    }
    // what is there is an array creation in a global scope
    else if (isSgGlobal(scope)) {
      cerr <<" ------->....... RuntimeInstrumentation :: WARNING - Scope not handled!!! : " << name << " : " << scope->class_name() << "\n\n\n\n" <<endl;
      // We need to add this new statement to the beginning of main
      // get the first statement in main as stmt
      stmt = mainFirst;
      scope=stmt->get_scope();
    }
    if (isSgBasicBlock(scope) ||
	isSgIfStmt(scope) ||
	isSgForStatement(scope)) {
      // build the function call : runtimeSystem-->createArray(params); ---------------------------
      SgExprListExp* arg_list = buildExprListExp();
      SgExpression* simplename = buildString(initName->get_name().str());
      appendExpression(arg_list, simplename);
      SgExpression* callName = buildString(initName->get_mangled_name().str());
      appendExpression(arg_list, callName);
#if 1
      if (derefExp)
    	  appendAddressAndSize(initName, derefExp, stmt, arg_list,2);
      else
#endif
    	  appendAddressAndSize(initName, varRefE, stmt, arg_list,2);

      SgExpression* filename = buildString(stmt->get_file_info()->get_filename());
      SgExpression* linenr = buildString(RoseBin_support::ToString(stmt->get_file_info()->get_line()));
      appendExpression(arg_list, filename);
      appendExpression(arg_list, linenr);

      SgExpression* linenrTransformed = buildString("x%%x");
      appendExpression(arg_list, linenrTransformed);

      appendExpression(arg_list, buildString(removeSpecialChar(stmt->unparseToString())));

      ROSE_ASSERT(roseAccessVariable);
      string symbolName2 = roseAccessVariable->get_name().str();
      //cerr << " >>>>>>>> Symbol Member: " << symbolName2 << endl;
      SgFunctionRefExp* memRef_r = buildFunctionRefExp(  roseAccessVariable);
      SgFunctionCallExp* funcCallExp = buildFunctionCallExp(memRef_r,
							    arg_list);
      SgExprStatement* exprStmt = buildExprStatement(funcCallExp);
      // insert new stmt (exprStmt) before (old) stmt
      insertStatementBefore(isSgStatement(stmt), exprStmt);
      string empty_comment = "";
      attachComment(exprStmt,empty_comment,PreprocessingInfo::before);
      string comment = "RS : Access Variable, paramaters : (name, mangl_name, address, sizeof(type), filename, line, line transformed, error Str)";
      attachComment(exprStmt,comment,PreprocessingInfo::before);
    } // basic block
    else if (isSgNamespaceDefinitionStatement(scope)) {
      cerr <<" ------------> RuntimeInstrumentation :: WARNING - Scope not handled!!! : " << name << " : " << scope->class_name() << "\n\n\n\n"<<endl;
    } else {
      cerr
	<< " -----------> RuntimeInstrumentation :: Surrounding Block is not Block! : "
	<< name << " : " << scope->class_name() << "  - " << stmt->unparseToString() << endl;
      ROSE_ASSERT(false);
    }
  } else {
    cerr
      << "RuntimeInstrumentation :: Surrounding Statement could not be found! "
      << stmt->class_name() << "  " << stmt->unparseToString() << endl;
    ROSE_ASSERT(false);
  }


}



void RtedTransformation::visit_isAssignInitializer(SgNode* n) {
  SgAssignInitializer* assign = isSgAssignInitializer(n);
  ROSE_ASSERT(assign);
  cerr << "\n\n???????????? Found assign init op : " << n->unparseToString() << endl;
  SgInitializedName* initName = isSgInitializedName(assign->get_parent());
  ROSE_ASSERT(initName);

  // ---------------------------------------------
  // we now know that this variable must be initialized
  // if we have not set this variable to be initialized yet,
  // we do so
  cerr  << ">>>>>>> Setting this var to be assign initialized : " << initName->unparseToString()
	<< "  and assignInit: " << assign->unparseToString()<<  endl;
  SgStatement* stmt = getSurroundingStatement(assign);
  ROSE_ASSERT(stmt);
  SgScopeStatement* scope = stmt->get_scope();
  ROSE_ASSERT(scope);
  SgVarRefExp* varRef = buildVarRefExp(initName, scope);
  ROSE_ASSERT(varRef);

  // dont do this if the variable is global
  if (isSgGlobal(initName->get_scope())) {
  } else if (isSgBasicBlock(initName->get_scope())) {
    //insertThisStatementLater[exprStmt]=stmt;
    bool ismalloc=false;
    variableIsInitialized[varRef]=std::pair<SgInitializedName*,bool>(initName,ismalloc);
    //cerr << "Inserting new statement : " << exprStmt->unparseToString() << endl;
    //cerr << "    after old statement : " << stmt->unparseToString() << endl;
  } else {
    cerr << " Cant determine scope : " << initName->get_scope()->class_name() << endl;
    exit(1);
  }

  // ---------------------------------------------
}
