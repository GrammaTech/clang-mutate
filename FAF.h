#ifndef CLANG_MUTATE_FAF_H
#define CLANG_MUTATE_FAF_H

// This class is a replacement for clang's FrontendActionFactory,
// with the property that the lifetimes of the CompilerInstances
// are not managed by the factory.  Instead, instantiated
// CompilerInstances are put into the global list TUs along with
// their AstTables; clang_mutate is then the owner of these
// instances.

#include "TU.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/PCHContainerOperations.h"
#include "clang/Tooling/Tooling.h"

class FAF : public clang::tooling::FrontendActionFactory
{
public:
    ~FAF() override;

    virtual bool runInvocation(clang::CompilerInvocation * Invocation,
                               clang::FileManager * Files,
                               std::shared_ptr<clang::PCHContainerOperations> PCHContainerOps,
                               clang::DiagnosticConsumer * DiagConsumer) override;
};

FAF::~FAF() {}

bool FAF::runInvocation(clang::CompilerInvocation * Invocation,
                        clang::FileManager * Files,
                        std::shared_ptr<clang::PCHContainerOperations> PCHContainerOps,
                        clang::DiagnosticConsumer * DiagConsumer)
{
    clang::CompilerInstance * Compiler = new clang::CompilerInstance;
    clang_mutate::TUs.push_back(clang_mutate::TU(Compiler));
        
    Compiler->setInvocation(Invocation);
    Compiler->setFileManager(Files);
    Compiler->createDiagnostics(DiagConsumer, /*ShouldOwnClient=*/false);
    Compiler->createSourceManager(*Files);
    
    std::unique_ptr<clang::FrontendAction> ScopedToolAction(create());

    if (!Compiler->hasDiagnostics())
        return false;

    const bool Success = Compiler->ExecuteAction(*ScopedToolAction);

    Files->clearStatCaches();
    return Success;
}

template <typename FactoryT>
 inline std::unique_ptr<FAF> newFAF(
     FactoryT *ConsumerFactory,
     clang::tooling::SourceFileCallbacks *Callbacks)
{
   class FAFAdapter : public FAF {
   public:
     explicit FAFAdapter(FactoryT *ConsumerFactory,
                         clang::tooling::SourceFileCallbacks *Callbacks)
       : ConsumerFactory(ConsumerFactory), Callbacks(Callbacks) {}
 
     clang::FrontendAction *create() override {
       return new ConsumerFactoryAdaptor(ConsumerFactory, Callbacks);
     }
 
   private:
     class ConsumerFactoryAdaptor : public clang::ASTFrontendAction {
     public:
       ConsumerFactoryAdaptor(FactoryT *ConsumerFactory,
                              clang::tooling::SourceFileCallbacks *Callbacks)
         : ConsumerFactory(ConsumerFactory), Callbacks(Callbacks) {}
 
       std::unique_ptr<clang::ASTConsumer>
           CreateASTConsumer(clang::CompilerInstance &, clang::StringRef) override {
         return ConsumerFactory->newASTConsumer();
       }
 
     protected:
       bool BeginSourceFileAction(clang::CompilerInstance &CI,
                                  clang::StringRef Filename) override {
         if (!clang::ASTFrontendAction::BeginSourceFileAction(CI, Filename))
           return false;
         if (Callbacks)
           return Callbacks->handleBeginSource(CI, Filename);
         return true;
       }
       void EndSourceFileAction() override {
         if (Callbacks)
           Callbacks->handleEndSource();
         clang::ASTFrontendAction::EndSourceFileAction();
       }
 
     private:
       FactoryT *ConsumerFactory;
       clang::tooling::SourceFileCallbacks *Callbacks;
     };
     FactoryT *ConsumerFactory;
     clang::tooling::SourceFileCallbacks *Callbacks;
   };
 
   return std::unique_ptr<FAF>(
       new FAFAdapter(ConsumerFactory, Callbacks));
}

#endif
