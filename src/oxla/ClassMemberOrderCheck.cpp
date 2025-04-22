#include "ClassMemberOrderCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::oxla {

namespace {
AST_MATCHER(CXXRecordDecl, isClassLike) {
  return Node.isClass() || Node.isStruct();
}
}

void ClassMemberOrderCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
    cxxRecordDecl(
      isClassLike(),
      isDefinition()
    ).bind("class"),
    this
  );
}

ClassMemberOrderCheck::MemberCategory
ClassMemberOrderCheck::classifyDecl(const Decl *D) {
  if (const auto *TD = dyn_cast<TypeDecl>(D)) {
    if (TD->getAccess() == AS_public)
      return MemberCategory::PublicTypes;
  }
  else if (const auto *FD = dyn_cast<FieldDecl>(D)) {
    return FD->getAccess() == AS_private ? MemberCategory::PrivateMembers
                                        : MemberCategory::PublicMembers;
  }
  else if (const auto *CD = dyn_cast<CXXConstructorDecl>(D); CD && CD->getAccess() == AS_public) {
    if (CD->isDefaultConstructor()) {
      return MemberCategory::PublicDefaultConstructors;
    }
    else if (CD->isCopyConstructor()) {
      return MemberCategory::PublicCopyConstructors;
    }
    else if (CD->isMoveConstructor()) {
      return MemberCategory::PublicMoveConstructors;
    }
    else if (CD->isConvertingConstructor(true)) {
      return MemberCategory::PublicConvertingConstructors;
    }
    else {
      return MemberCategory::PublicOtherConstructors;
    }
  }
  else if (const auto *CD = dyn_cast<CXXConstructorDecl>(D); CD && CD->getAccess() == AS_private) {
    return MemberCategory::PrivateConstructors;
  }
  else if (const auto *MD = dyn_cast<CXXMethodDecl>(D)) {
    if (MD->getAccess() == AS_private) {
      return MemberCategory::PrivateMethods;
    }

    if (isa<CXXDestructorDecl>(MD)) {
      return MemberCategory::PublicDestructors;
    }

    // Conversion operators
    if (isa<CXXConversionDecl>(MD))
      return MemberCategory::PublicConversionOperators;

    if (MD->isOverloadedOperator()) {
      // Assignment operators
      if (MD->isCopyAssignmentOperator())
        return MemberCategory::PublicCopyAssignment;
      if (MD->isMoveAssignmentOperator())
        return MemberCategory::PublicMoveAssignment;

      // Specific operator types
      const auto& Op = MD->getOverloadedOperator();
      switch (Op) {
      case OO_Subscript:
        return MemberCategory::PublicSubscriptOperators;
      case OO_Call:
        return MemberCategory::PublicFunctionCallOperators;
      default:
        return MemberCategory::PublicOtherOperators;
      }
    }

    if (MD->isInstance() && (MD->getName().starts_with("get") ||
                            MD->getName().starts_with("set"))) {
      return MemberCategory::PublicAccessors;
                            }

    if (MD->isStatic()) {
      return MemberCategory::PublicStaticFunctions;
    }

    if (MD->isVirtual()) {
      return MemberCategory::PublicVirtualFunctions;
    }

    return MemberCategory::PublicMethods;
  }
  else if (isa<FriendDecl>(D)) {
    return MemberCategory::FriendDeclarations;
  }

  return MemberCategory::Invalid;
}

std::string ClassMemberOrderCheck::getCategoryName(MemberCategory Cat) {
  switch (Cat) {
// Existing categories
    case MemberCategory::PublicTypes: return "public types/enums";
    case MemberCategory::PrivateMembers: return "private members";
    case MemberCategory::PublicMembers: return "public members";

    // Constructors
    case MemberCategory::PublicDefaultConstructors:
      return "public default constructors";
    case MemberCategory::PublicCopyConstructors:
      return "public copy constructors";
    case MemberCategory::PublicMoveConstructors:
      return "public move constructors";
    case MemberCategory::PublicConvertingConstructors:
      return "public converting constructors";
    case MemberCategory::PublicOtherConstructors:
      return "public other constructors";
    case MemberCategory::PublicDestructors:
      return "public destructors";

    // Operators
    case MemberCategory::PublicCopyAssignment:
      return "public copy assignment operators";
    case MemberCategory::PublicMoveAssignment:
      return "public move assignment operators";
    case MemberCategory::PublicConversionOperators:
      return "public conversion operators";
    case MemberCategory::PublicSubscriptOperators:
      return "public subscript operators";
    case MemberCategory::PublicFunctionCallOperators:
      return "public function call operators";
    case MemberCategory::PublicOtherOperators:
      return "public other operators";

    // public functions
    case MemberCategory::PublicAccessors: return "public getters/setters";
    case MemberCategory::PublicStaticFunctions: return "public static functions";
    case MemberCategory::PublicVirtualFunctions: return "public virtual functions";
    case MemberCategory::PublicMethods: return "public methods";

    // private constructors and functions
    case MemberCategory::PrivateConstructors: return "private constructors";
    case MemberCategory::PrivateMethods: return "private methods";

    case MemberCategory::FriendDeclarations: return "friend declarations";
  default: return "invalid";
  }
}

void ClassMemberOrderCheck::reportOutOfOrder(
    const CXXRecordDecl *Record,
    const Decl *CurrentDecl,
    MemberCategory CurrentCategory,
    MemberCategory LastCategory) {
  auto Diag = diag(
    CurrentDecl->getLocation(),
    "declaration of %0 out of order (should come before %1)")
    << getCategoryName(CurrentCategory) << getCategoryName(LastCategory);

  // Add note showing where the previous category ended
  if (LastCategory != MemberCategory::Invalid) {
    for (const auto *D : Record->decls()) {
      if (classifyDecl(D) == LastCategory) {
        Diag << FixItHint::CreateInsertion(
          D->getEndLoc(),
          "\n// " + getCategoryName(CurrentCategory) + "\n");
        break;
      }
    }
  }
}

void ClassMemberOrderCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Record = Result.Nodes.getNodeAs<CXXRecordDecl>("class");
  if (!Record || Record->isImplicit() || !Record->isCompleteDefinition())
    return;

  MemberCategory LastCategory = MemberCategory::Invalid;
  MemberCategory CurrentCategory;

  for (const auto *D : Record->decls()) {
    // Skip implicit and non-relevant declarations
    if (D->isImplicit() || isa<AccessSpecDecl>(D) || isa<EmptyDecl>(D))
      continue;

    CurrentCategory = classifyDecl(D);
    if (CurrentCategory == MemberCategory::Invalid)
      continue;

    if (LastCategory != MemberCategory::Invalid &&
        CurrentCategory < LastCategory) {
      reportOutOfOrder(Record, D, CurrentCategory, LastCategory);
    }

    LastCategory = CurrentCategory;
  }
}

} // namespace clang::tidy::readability
