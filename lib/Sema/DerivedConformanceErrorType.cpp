//===--- DerivedConformanceErrorType.cpp - Derived ErrorType --------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  This file implements implicit derivation of the ErrorType
//  protocol.
//
//===----------------------------------------------------------------------===//

#include "TypeChecker.h"
#include "DerivedConformances.h"
#include "swift/AST/ArchetypeBuilder.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Stmt.h"
#include "swift/AST/Expr.h"
#include "swift/AST/Types.h"

using namespace swift;
using namespace DerivedConformance;

static bool canDeriveConformance(NominalTypeDecl *type) {
  // The type must be an enum.
  return isa<EnumDecl>(type);
}

static void deriveBodyErrorType_enum_domain(AbstractFunctionDecl *domainDecl) {
  // enum SomeEnum {
  //   @derived
  //   var domain: String {
  //     return "\(self.dynamicType)"
  //   }
  // }

  auto &C = domainDecl->getASTContext();

  auto selfRef = createSelfDeclRef(domainDecl);
  DynamicTypeExpr *selfDynamicType
    = new (C) DynamicTypeExpr(selfRef, SourceLoc(), Type());
  selfDynamicType->setImplicit();
  
  Expr *segmentElts[] = {
    selfDynamicType,
  };
  auto segments = C.AllocateCopy(segmentElts);
  auto string = new (C) InterpolatedStringLiteralExpr(SourceLoc(), segments);
  string->setImplicit();
  
  auto ret = new (C) ReturnStmt(SourceLoc(), string, /*implicit*/ true);
  auto body = BraceStmt::create(C, SourceLoc(),
                              ASTNode(ret),
                              SourceLoc());
  domainDecl->setBody(body);
}

static ValueDecl *deriveErrorType_enum_domain(TypeChecker &tc,
                                              EnumDecl *enumDecl) {
  // enum SomeEnum {
  //   @derived
  //   var domain: String {
  //     return "\(self.dynamicType)"
  //   }
  // }
  
  ASTContext &C = tc.Context;
  
  auto stringTy = C.getStringDecl()->getDeclaredType();
  Type enumType = enumDecl->getDeclaredTypeInContext();

  // Define the getter.
  auto getterDecl = declareDerivedPropertyGetter(tc, enumDecl, enumType,
                                                 stringTy, stringTy);
  getterDecl->setBodySynthesizer(&deriveBodyErrorType_enum_domain);
  
  // Define the property.
  VarDecl *propDecl;
  PatternBindingDecl *pbDecl;
  std::tie(propDecl, pbDecl)
    = declareDerivedReadOnlyProperty(tc, enumDecl, C.Id_domain,
                                     stringTy, stringTy,
                                     getterDecl);
  
  enumDecl->addMember(getterDecl);
  enumDecl->addMember(propDecl);
  enumDecl->addMember(pbDecl);
  return propDecl;
}

static void deriveBodyErrorType_enum_code(AbstractFunctionDecl *codeDecl) {
  // enum SomeEnum {
  //   case A,B,C,D
  //
  //   @derived
  //   var code: Int {
  //     switch self {
  //     case A: return 0
  //     case B: return 1
  //     case C: return 2
  //     ...
  //     }
  //   }
  // }
  //
  // TODO: Some convenient way to override the code if that's desired.

  ASTContext &C = codeDecl->getASTContext();
  auto enumDecl = cast<EnumDecl>(codeDecl->getDeclContext());
  Type enumType = enumDecl->getDeclaredTypeInContext();

  SmallVector<CaseStmt*, 4> cases;
  SmallString<11> strBuf;

  unsigned code = 0;
  for (auto elt : enumDecl->getAllElements()) {
    auto pat = new (C) EnumElementPattern(TypeLoc::withoutLoc(enumType),
                                          SourceLoc(), SourceLoc(),
                                          Identifier(), elt, nullptr);
    pat->setImplicit();
    
    auto labelItem =
      CaseLabelItem(/*IsDefault=*/false, pat, SourceLoc(), nullptr);

    {
      strBuf.clear();
      llvm::raw_svector_ostream os(strBuf);
      os << code;
      os.flush();
    }
    
    auto codeStr = C.AllocateCopy(StringRef(strBuf));

    auto returnExpr = new (C) IntegerLiteralExpr(codeStr, SourceLoc(),
                                                 /*implicit*/ true);
    auto returnStmt = new (C) ReturnStmt(SourceLoc(), returnExpr,
                                         /*implicit*/ true);
    
    auto body = BraceStmt::create(C, SourceLoc(),
                                  ASTNode(returnStmt), SourceLoc());

    cases.push_back(CaseStmt::create(C, SourceLoc(), labelItem,
                                     /*HasBoundDecls=*/false, SourceLoc(),
                                     body));
    
    ++code;
  }
  
  Stmt *bodyStmt;
  // If the enum is empty, simply return zero. (It doesn't really matter, since
  // the enum can't be instantiated regardless.)
  if (cases.empty()) {
    static const char zero[] = "0";
    auto returnExpr = new (C) IntegerLiteralExpr(zero, SourceLoc(),
                                                 /*implicit*/ true);
    bodyStmt = new (C) ReturnStmt(SourceLoc(), returnExpr,
                                  /*implicit*/ true);
  } else {
    auto selfRef = createSelfDeclRef(codeDecl);
    bodyStmt = SwitchStmt::create(LabeledStmtInfo(), SourceLoc(), selfRef,
                                  SourceLoc(), cases, SourceLoc(), C);
  }
  auto body = BraceStmt::create(C, SourceLoc(),
                                ASTNode(bodyStmt),
                                SourceLoc());

  codeDecl->setBody(body);
}

static ValueDecl *deriveErrorType_enum_code(TypeChecker &tc,
                                            EnumDecl *enumDecl) {
  // enum SomeEnum {
  //   case A,B,C,D
  //
  //   @derived
  //   var code: Int {
  //     switch self {
  //     case A: return 0
  //     case B: return 1
  //     case C: return 2
  //     ...
  //     }
  //   }
  // }
  
  ASTContext &C = tc.Context;
  
  auto intTy = C.getIntDecl()->getDeclaredType();
  Type enumType = enumDecl->getDeclaredTypeInContext();

  // Define the getter.
  auto getterDecl = declareDerivedPropertyGetter(tc, enumDecl, enumType,
                                                 intTy, intTy);
  getterDecl->setBodySynthesizer(&deriveBodyErrorType_enum_code);
  
  // Define the property.
  VarDecl *propDecl;
  PatternBindingDecl *pbDecl;
  std::tie(propDecl, pbDecl)
    = declareDerivedReadOnlyProperty(tc, enumDecl, C.Id_code,
                                     intTy, intTy,
                                     getterDecl);
  
  enumDecl->addMember(getterDecl);
  enumDecl->addMember(propDecl);
  enumDecl->addMember(pbDecl);
  return propDecl;

}

ValueDecl *DerivedConformance::deriveErrorType(TypeChecker &tc,
                                               NominalTypeDecl *type,
                                               ValueDecl *requirement) {
  if (!canDeriveConformance(type))
    return nullptr;
  
  auto enumType = cast<EnumDecl>(type);
  
  if (requirement->getName() == tc.Context.Id_domain)
    return deriveErrorType_enum_domain(tc, enumType);
  else if (requirement->getName() == tc.Context.Id_code)
    return deriveErrorType_enum_code(tc, enumType);
  
  tc.diagnose(requirement->getLoc(),
              diag::broken_errortype_requirement);
  return nullptr;
}
