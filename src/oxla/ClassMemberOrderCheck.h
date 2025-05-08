#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_OXLA_CLASSMEMBERORDERCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_OXLA_CLASSMEMBERORDERCHECK_H

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyCheck.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

namespace clang::tidy::oxla {

/// Checking order of class members declaration
class ClassMemberOrderCheck : public ClangTidyCheck {
public:
  ClassMemberOrderCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

  enum class MemberCategory {
    PublicTypes,
    PrivateMembers,
    PublicMembers,

    PublicDefaultConstructors,
    PublicCopyConstructors,
    PublicMoveConstructors,
    PublicConvertingConstructors,
    PublicOtherConstructors,
    PublicDestructors,

    PublicCopyAssignment,
    PublicMoveAssignment,
    PublicConversionOperators,
    PublicSubscriptOperators,
    PublicFunctionCallOperators,
    PublicOtherOperators,

    PublicAccessors,
    PublicStaticFunctions,
    PublicVirtualFunctions,
    PublicMethods,

    PrivateConstructors,
    PrivateMethods,

    FriendDeclarations,
    Invalid
  };
    static MemberCategory classifyDecl(const Decl *D);
private:

  std::string getCategoryName(MemberCategory Cat);
  DiagnosticBuilder reportOutOfOrder(const CXXRecordDecl *Record,
                        const Decl *CurrentDecl,
                        MemberCategory CurrentCategory,
                        MemberCategory LastCategory);
};

} // namespace clang::tidy::oxla

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_OXLA_CLASSMEMBERORDERCHECK_H
