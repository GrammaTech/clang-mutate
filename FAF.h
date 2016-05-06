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
    static clang_mutate::TURef next_tuid = 0;
    clang::CompilerInstance * Compiler = new clang::CompilerInstance;
    clang_mutate::TURef tuid = next_tuid++;
    clang_mutate::TUs[tuid] = new clang_mutate::TU(tuid);

    clang_mutate::tu_in_progress = clang_mutate::TUs[tuid];

    Compiler->setInvocation(Invocation);
    Compiler->setFileManager(Files);
    Compiler->createDiagnostics(DiagConsumer, /*ShouldOwnClient=*/false);
    Compiler->createSourceManager(*Files);

    std::unique_ptr<clang::FrontendAction> ScopedToolAction(create());

    if (!Compiler->hasDiagnostics())
        return false;

    const bool Success = Compiler->ExecuteAction(*ScopedToolAction);

    clang::SourceManager & sm = Compiler->getSourceManager();
    clang_mutate::tu_in_progress->source
        = sm.getBufferData(sm.getMainFileID()).str();

    Files->clearStatCaches();
    delete Compiler;
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
