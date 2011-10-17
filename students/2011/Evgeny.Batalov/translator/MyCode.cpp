#include <iostream>
#include "MyCode.h"

MyCode::MyCode() {
    using namespace mathvm;
}

mathvm::Status* MyCode::execute(std::vector<mathvm::Var*, std::allocator<mathvm::Var*> >& vars) {
    //FIXME: implement me!
    return 0;
}

void MyCode::disassemble(std::ostream& out, mathvm::FunctionFilter *f) {
    using namespace mathvm;
    std::vector<uint16_t>::const_iterator fit = functionIds.begin();
    for(; fit != functionIds.end(); ++fit) {
        TranslatedFunction *ft = functionById(*fit);
        BytecodeFunction *fb = dynamic_cast<BytecodeFunction*>(ft);
        std::cout << std::endl  << *fit << ":" << fb->name() << ":"  << std::endl;
        fb->disassemble(out);
    }
}


