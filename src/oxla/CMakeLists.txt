add_library(OxlaModuleClangTidyCheck MODULE "")
target_compile_options(OxlaModuleClangTidyCheck PRIVATE -fno-rtti)
target_include_directories(OxlaModuleClangTidyCheck
    PRIVATE
        ${CLANG_INCLUDE_DIRS}
        ${LLVM_INCLUDE_DIRS}
)
target_sources(OxlaModuleClangTidyCheck
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/ClassMemberOrderCheck.cpp
        ${CMAKE_CURRENT_LIST_DIR}/OxlaTidyModule.cpp)
