#include "ClassMemberOrderCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::oxla {

namespace {
AST_MATCHER(CXXRecordDecl, isClassLike)
{
    return Node.isClass() || Node.isStruct();
}

class MemberSorter
{
public:
    MemberSorter(SourceManager& SM, const LangOptions& LO) : SM(SM), LO(LO)
    {
    }

    struct MemberInfo {
        const Decl*                           Decl;
        ClassMemberOrderCheck::MemberCategory Category;
        SourceRange                           Range;
        std::string                           Text;
    };

    void sortMembers(const CXXRecordDecl* Record, DiagnosticBuilder& Diag)
    {
        std::vector<MemberInfo> members;

        // Collect all relevant members with their categories
        for (const auto* D : Record->decls()) {
            if (D->isImplicit() || isa<AccessSpecDecl>(D) || isa<EmptyDecl>(D))
                continue;

            auto cat = ClassMemberOrderCheck::classifyDecl(D);
            if (cat == ClassMemberOrderCheck::MemberCategory::Invalid)
                continue;

            members.push_back({D, cat, D->getSourceRange(), getDeclText(D)});
        }

        // Sort members according to your category ordering
        std::stable_sort(members.begin(), members.end(), [](const MemberInfo& a, const MemberInfo& b) { return a.Category < b.Category; });

        // Generate the new text with proper access specifiers
        std::string                           newText;
        ClassMemberOrderCheck::MemberCategory lastCat = ClassMemberOrderCheck::MemberCategory::Invalid;

        for (const auto& member : members) {
            if (member.Category != lastCat) {
                newText += getAccessSpecifier(member.Category) + "\n";
                lastCat = member.Category;
            }
            newText += member.Text + "\n";
        }

        // Create replacement for the entire class body
        SourceLocation bodyStart = Record->getBraceRange().getBegin().getLocWithOffset(1);
        SourceLocation bodyEnd   = Record->getBraceRange().getEnd().getLocWithOffset(-1);

        Diag.AddFixItHint(FixItHint::CreateReplacement(CharSourceRange::getCharRange(bodyStart, bodyEnd), newText));
    }

private:
    std::string getDeclText(const Decl* D)
    {
        return Lexer::getSourceText(CharSourceRange::getTokenRange(D->getSourceRange()), SM, LO).str();
    }

    std::string getAccessSpecifier(ClassMemberOrderCheck::MemberCategory cat)
    {
        switch (cat) {
            case ClassMemberOrderCheck::MemberCategory::PrivateMembers:
            case ClassMemberOrderCheck::MemberCategory::PrivateConstructors:
            case ClassMemberOrderCheck::MemberCategory::PrivateMethods:
                return "private:";
            default:
                return "public:";
        }
    }

    SourceManager&     SM;
    const LangOptions& LO;
};
}  // namespace

void ClassMemberOrderCheck::registerMatchers(MatchFinder* Finder)
{
    Finder->addMatcher(cxxRecordDecl(isClassLike(), isDefinition()).bind("class"), this);
}

ClassMemberOrderCheck::MemberCategory ClassMemberOrderCheck::classifyDecl(const Decl* D)
{
    if (const auto* TD = dyn_cast<TypeDecl>(D)) {
        if (TD->getAccess() == AS_public)
            return MemberCategory::PublicTypes;
    } else if (const auto* FD = dyn_cast<FieldDecl>(D)) {
        return FD->getAccess() == AS_private ? MemberCategory::PrivateMembers : MemberCategory::PublicMembers;
    } else if (const auto* CD = dyn_cast<CXXConstructorDecl>(D); CD && CD->getAccess() == AS_public) {
        if (CD->isDefaultConstructor()) {
            return MemberCategory::PublicDefaultConstructors;
        } else if (CD->isCopyConstructor()) {
            return MemberCategory::PublicCopyConstructors;
        } else if (CD->isMoveConstructor()) {
            return MemberCategory::PublicMoveConstructors;
        } else if (CD->isConvertingConstructor(true)) {
            return MemberCategory::PublicConvertingConstructors;
        } else {
            return MemberCategory::PublicOtherConstructors;
        }
    } else if (const auto* CD = dyn_cast<CXXConstructorDecl>(D); CD && CD->getAccess() == AS_private) {
        return MemberCategory::PrivateConstructors;
    } else if (const auto* MD = dyn_cast<CXXMethodDecl>(D)) {
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

        if (MD->isInstance() && (MD->getName().starts_with("get") || MD->getName().starts_with("set"))) {
            return MemberCategory::PublicAccessors;
        }

        if (MD->isStatic()) {
            return MemberCategory::PublicStaticFunctions;
        }

        if (MD->isVirtual()) {
            return MemberCategory::PublicVirtualFunctions;
        }

        return MemberCategory::PublicMethods;
    } else if (isa<FriendDecl>(D)) {
        return MemberCategory::FriendDeclarations;
    }

    return MemberCategory::Invalid;
}

std::string ClassMemberOrderCheck::getCategoryName(MemberCategory Cat)
{
    switch (Cat) {
            // Existing categories
        case MemberCategory::PublicTypes:
            return "public types/enums";
        case MemberCategory::PrivateMembers:
            return "private members";
        case MemberCategory::PublicMembers:
            return "public members";

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
        case MemberCategory::PublicAccessors:
            return "public getters/setters";
        case MemberCategory::PublicStaticFunctions:
            return "public static functions";
        case MemberCategory::PublicVirtualFunctions:
            return "public virtual functions";
        case MemberCategory::PublicMethods:
            return "public methods";

        // private constructors and functions
        case MemberCategory::PrivateConstructors:
            return "private constructors";
        case MemberCategory::PrivateMethods:
            return "private methods";

        case MemberCategory::FriendDeclarations:
            return "friend declarations";
        default:
            return "invalid";
    }
}

DiagnosticBuilder ClassMemberOrderCheck::reportOutOfOrder(const CXXRecordDecl* Record,
                                                          const Decl*          CurrentDecl,
                                                          MemberCategory       CurrentCategory,
                                                          MemberCategory       LastCategory)
{
    auto diag_builder = diag(CurrentDecl->getLocation(), "declaration of %0 out of order (should come before %1)")
                << getCategoryName(CurrentCategory) << getCategoryName(LastCategory);

    // Add note showing where the previous category ended
    if (LastCategory != MemberCategory::Invalid) {
        for (const auto* D : Record->decls()) {
            if (classifyDecl(D) == LastCategory) {
                diag_builder << FixItHint::CreateInsertion(D->getEndLoc(), "\n// " + getCategoryName(CurrentCategory) + "\n");
                break;
            }
        }
    }

    return diag_builder;
}

void ClassMemberOrderCheck::check(const MatchFinder::MatchResult& Result)
{
    const auto* Record = Result.Nodes.getNodeAs<CXXRecordDecl>("class");
    if (!Record || Record->isImplicit() || !Record->isCompleteDefinition())
        return;

    auto LastCategory = MemberCategory::Invalid;

    for (const auto* D : Record->decls()) {
        // Skip implicit and non-relevant declarations
        if (D->isImplicit() || isa<AccessSpecDecl>(D) || isa<EmptyDecl>(D))
            continue;

        auto CurrentCategory = classifyDecl(D);
        if (CurrentCategory == MemberCategory::Invalid)
            continue;

        if (LastCategory != MemberCategory::Invalid && CurrentCategory < LastCategory) {
            auto diag = reportOutOfOrder(Record, D, CurrentCategory, LastCategory);

            // Apply fixes
            MemberSorter sorter(*Result.SourceManager, Result.Context->getLangOpts());
            sorter.sortMembers(Record, diag);
        }

        LastCategory = CurrentCategory;
    }
}

}  // namespace clang::tidy::oxla
