#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"
#include "ClassMemberOrderCheck.h"

namespace clang::tidy {
namespace oxla {
class OxlaTidyModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<ClassMemberOrderCheck>(
    "oxla-class-member-order");
  }
};

} // namespace oxla

// Register the OxlaModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<oxla::OxlaTidyModule>
    X("oxla-class-member-order", "Check order of class members declarations.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the OxlaModule.
volatile int OxlaModuleAnchorSource = 0;

} // namespace clang::tidy
