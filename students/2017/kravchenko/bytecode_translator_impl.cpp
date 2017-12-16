#include "mathvm.h"
#include "parser.h"
#include "visitors.h"
#include "bytecode_translator.h"

#include <iostream>
#include <stdarg.h>

namespace mathvm {

// BytecodeTranslator

Status* BytecodeTranslatorImpl::translate(const string& program, Code* *code)
{
    Status *res;
    Parser parser;
    res = parser.parseProgram(program);

    if (res->isError())
        return res;

    AstFunction *top = parser.top();

    BytecodeVisitor b_visitor;
    b_visitor.translate(top);

    *code = b_visitor.get_code();

    return res;
}

}

using namespace mathvm;

// BytecodeVisitor

bool BytecodeVisitor::isNative(FunctionNode *node)
{
    return (node->body()->nodes() > 0 && dynamic_cast<NativeCallNode *>(node->body()->nodeAt(0)));
}

void BytecodeVisitor::registerScopes(Scope *s)
{
    _scope_map[s] = _scope_map.size();

    static bool isMain = true;

    for (Scope::VarIterator var_it(s); var_it.hasNext();) {
        AstVar *var = var_it.next();
        _var_map[s][var->name()] = _var_map[s].size();
        if (isMain)
            _code->addVarId(var->name(), _var_map[s][var->name()]);
//        fprintf(stderr, "registered variable '%s' in scope %u with id %u\n", var->name().c_str(), _scope_map[s], _var_map[s][var->name()]);
    }

    isMain = false;

    for (uint32_t i = 0; i < s->childScopeNumber(); i++)
        registerScopes(s->childScopeAt(i));
}

void BytecodeVisitor::registerFunctions(AstFunction *a_fun)
{
//    fprintf(stderr, "registering function %s\n", a_fun->name().c_str());

    FunctionNode *node = a_fun->node();
    _funcs.push_back(a_fun);
    BytecodeFunction *fun = new BytecodeFunction(a_fun);
    _code->addFunction(fun);
    _funIdMap[a_fun->name()] = fun->id();

    for (Scope::FunctionIterator fun_it(node->body()->scope()); fun_it.hasNext();) {
        AstFunction *fun = fun_it.next();
        registerFunctions(fun);
    }
}

void BytecodeVisitor::translate(AstFunction *a_fun)
{
    _code = new BytecodeInterpreter();
    // get functions and scopes ids.
    registerScopes(a_fun->owner());
    registerFunctions(a_fun);

    _dlHandler = dlopen(NULL, RTLD_LAZY | RTLD_NODELETE);

    for (AstFunction *fun : _funcs) {
        _fun = nullptr;
        _scope = nullptr;
        translateAstFunction(fun);
        assert(types.size() == 0);
        assert(_scopeSizes.size() == 0);
    }

//    fprintf(stderr, "\n\n\n");
}

void BytecodeVisitor::translateAstFunction(AstFunction *a_fun)
{
    _fun = (BytecodeFunction *)_code->functionByName(a_fun->name());
    _fun->setScopeId(_scope_map[a_fun->scope()]);
    _fun->setLocalsNumber(a_fun->node()->body()->scope()->variablesCount());

    _scope = a_fun->scope();

    a_fun->node()->visit(this);

//    fprintf(stderr, "translated ast function %s:\n", _fun->name().c_str());
//    fprintf(stderr, "    id = %d\n", _fun->id());
//    fprintf(stderr, "    locals = %d\n", _fun->localsNumber());
//    fprintf(stderr, "    params = %d\n", _fun->parametersNumber());
//    fprintf(stderr, "    scopeId = %d\n", _fun->scopeId());
//    _fun->bytecode()->dump(std::cout);
}

void BytecodeVisitor::addBranch(Instruction insn, Label &l)
{
    switch (insn) {
        case BC_IFICMPNE:
        case BC_IFICMPE:
        case BC_IFICMPG:
        case BC_IFICMPGE:
        case BC_IFICMPL:
        case BC_IFICMPLE:
            assert(types.top() == VT_INT);
            types.pop();
            assert(types.top() == VT_INT);
            types.pop();
            break;
        case BC_JA:
            break;
        default:
            assert(false);
    }

    _fun->bytecode()->addBranch(insn, l);
}

void BytecodeVisitor::addInsn(Instruction insn)
{
    _fun->bytecode()->addInsn(insn);
    switch (insn) {
// ints
        case BC_ILOAD:
        case BC_ILOAD0:
        case BC_ILOAD1:
        case BC_ILOADM1:
            types.push(VT_INT);
            break;

        case BC_IADD:
        case BC_ISUB:
        case BC_IMUL:
        case BC_IDIV:
        case BC_IMOD:
        case BC_IAOR:
        case BC_IAAND:
        case BC_IAXOR:
        case BC_ICMP:
            assert(types.top() == VT_INT);
            types.pop();
            assert(types.top() == VT_INT);
            types.pop();
            types.push(VT_INT);
            break;

        case BC_INEG:
            assert(types.top() == VT_INT);
            break;

        case BC_IPRINT:
            assert(types.top() == VT_INT);
            types.pop();
            break;

        case BC_LOADIVAR:
        case BC_LOADIVAR0:
        case BC_LOADIVAR1:
        case BC_LOADIVAR2:
        case BC_LOADIVAR3:
        case BC_LOADCTXIVAR:
            types.push(VT_INT);
            break;

        case BC_STOREIVAR:
        case BC_STOREIVAR0:
        case BC_STOREIVAR1:
        case BC_STOREIVAR2:
        case BC_STOREIVAR3:
        case BC_STORECTXIVAR:
            assert(types.top() == VT_INT);
            types.pop();
            break;

        case BC_IFICMPNE:
        case BC_IFICMPE:
        case BC_IFICMPG:
        case BC_IFICMPGE:
        case BC_IFICMPL:
        case BC_IFICMPLE:
            assert(false); // should add them with addBranch
            break;

// doubles
        case BC_DLOAD:
        case BC_DLOAD0:
        case BC_DLOAD1:
        case BC_DLOADM1:
            types.push(VT_DOUBLE);
            break;

        case BC_DADD:
        case BC_DSUB:
        case BC_DMUL:
        case BC_DDIV:
            assert(types.top() == VT_DOUBLE);
            types.pop();
            assert(types.top() == VT_DOUBLE);
            types.pop();
            types.push(VT_DOUBLE);
            break;

        case BC_DCMP:
            assert(types.top() == VT_DOUBLE);
            types.pop();
            assert(types.top() == VT_DOUBLE);
            types.pop();
            types.push(VT_INT);
            break;

        case BC_DNEG:
            assert(types.top() == VT_DOUBLE);
            break;

        case BC_DPRINT:
            assert(types.top() == VT_DOUBLE);
            types.pop();
            break;

        case BC_LOADDVAR:
        case BC_LOADDVAR0:
        case BC_LOADDVAR1:
        case BC_LOADDVAR2:
        case BC_LOADDVAR3:
        case BC_LOADCTXDVAR:
            types.push(VT_DOUBLE);
            break;

        case BC_STOREDVAR:
        case BC_STOREDVAR0:
        case BC_STOREDVAR1:
        case BC_STOREDVAR2:
        case BC_STOREDVAR3:
        case BC_STORECTXDVAR:
            assert(types.top() == VT_DOUBLE);
            types.pop();
            break;

// strings
        case BC_SLOAD:
        case BC_SLOAD0:
            types.push(VT_STRING);
            break;

        case BC_SPRINT:
            assert(types.top() == VT_STRING);
            types.pop();
            break;

        case BC_LOADSVAR:
        case BC_LOADSVAR0:
        case BC_LOADSVAR1:
        case BC_LOADSVAR2:
        case BC_LOADSVAR3:
        case BC_LOADCTXSVAR:
            types.push(VT_STRING);
            break;

        case BC_STORESVAR:
        case BC_STORESVAR0:
        case BC_STORESVAR1:
        case BC_STORESVAR2:
        case BC_STORESVAR3:
        case BC_STORECTXSVAR:
            assert(types.top() == VT_STRING);
            types.pop();
            break;

// casts
        case BC_I2D:
            assert(types.top() == VT_INT);
            types.pop();
            types.push(VT_DOUBLE);
            break;
        case BC_D2I:
            assert(types.top() == VT_DOUBLE);
            types.pop();
            types.push(VT_INT);
            break;
        case BC_S2I:
            assert(types.top() == VT_STRING);
            types.pop();
            types.push(VT_INT);
            break;

// any
        case BC_SWAP:
            {
                VarType t1 = types.top();
                types.pop();
                VarType t2 = types.top();
                types.pop();
                types.push(t1);
                types.push(t2);
            }
            break;
        case BC_POP:
            types.pop();
            break;

// not modifying stack
        case BC_JA:
        case BC_STOP:
        case BC_BREAK:
        case BC_RETURN:
        case BC_CALL: // these 2 below are special cases
        case BC_CALLNATIVE:
            break;

        default:
            fprintf(stderr, "unknown instruction %d\n", insn);
            assert(false);
            break;
    }
}

/*
 * This functions tells us which types can arguments of @op have
 * and which types can be returned as result
 *
 * res[0] -- possible types of the result
 * res[1] -- possible types of the first argument
 * res[2] -- possible types of the second argument
 * res[3] -- 0 if arguments can have different types,
 *           1 otherwise
 *
 * if operation only has 1 argument, res[2] can be ignored
 */
std::vector<uint8_t> BytecodeVisitor::opResType(TokenKind op)
{
    std::vector<uint8_t> res;
    res.push_back(0);
    res.push_back(0);
    res.push_back(0);
    res.push_back(0);

    const uint8_t I = 1 << VT_INT;
    const uint8_t D = 1 << VT_DOUBLE;
    const uint8_t S = 1 << VT_STRING;

    switch (op) {
        // there operations only can be applied to integers
        case tAOR:
        case tAAND:
        case tAXOR:
        case tMOD:
        case tRANGE:
            res[0] = res[1] = res[2] = I;
            res[3] = 1;
            break;
        case tNOT:
        case tADD:
        case tSUB:
        case tMUL:
        case tDIV:
            res[0] = res[1] = res[2] = I | D;
            res[3] = 0;
            break;
        case tEQ:
        case tNEQ:
        case tGT:
        case tGE:
        case tLT:
        case tLE:
            res[0] = res[1] = res[2] = I | D | S;
            res[3] = 1; // Only compare same types
            break;
        default:
            fprintf(stderr, "unknown operation %d\n", op);
            assert(false);

    }

    return res;
}

void BytecodeVisitor::convertType(VarType to)
{
    VarType from = types.top();

    if (from == to)
        return;
    if (from == VT_INT && to == VT_DOUBLE) {
        addInsn(BC_I2D);
        return;
    }
    if (from == VT_DOUBLE && to == VT_INT) {
        addInsn(BC_D2I);
        return;
    }
    if (from == VT_STRING && to == VT_INT) {
        addInsn(BC_S2I);
        return;
    }

    fprintf(stderr, "trying to convert from %d to %d\n", from, to);
    assert(false);
}

// correct types on stack to make them
// 1) equal
// 2) correspond resTypes
//
// @n -- 1 or 2 -- number of arguments
void BytecodeVisitor::correctTypes(int n, std::vector<uint8_t> resTypes)
{
    assert(1 <= n && n <= 2);

#define BAD_TYPE(id, type) (((resTypes[id] & (1 << type)) == 0) && (resTypes[id] == 1))

    if (n == 1) {
        VarType argType = types.top();

        if (BAD_TYPE(1, argType)) {
            fprintf(stderr, "can not correct type %d to types %d", argType, resTypes[1]);
            assert(false);
        }

        if ((resTypes[1] & (1 << argType)) == 0)
            convertType(VT_INT); // conver to int by default
    } else {
        VarType rhs = types.top();
        types.pop();
        VarType lhs = types.top();
        types.push(rhs);

        if (BAD_TYPE(1, lhs) || BAD_TYPE(2, rhs)) {
            fprintf(stderr, "can not correct type %d to types %d", lhs, resTypes[1]);
            fprintf(stderr, "can not correct type %d to types %d", rhs, resTypes[2]);
            assert(false);
        }

        if (lhs == rhs)
            return;

        VarType finalType = VT_DOUBLE;
        if (rhs == VT_STRING || lhs == VT_STRING)
            finalType = VT_INT;

        if (rhs != finalType)
            convertType(finalType);
        if (lhs != finalType) {
            addInsn(BC_SWAP);
            convertType(finalType);
            addInsn(BC_SWAP);
        }
    }

#undef BAD_TYPE
}

void BytecodeVisitor::binaryMathOp(BinaryOpNode *node)
{
    node->left()->visit(this);
    node->right()->visit(this);

    TokenKind op = node->kind();

    correctTypes(2, opResType(op));
    VarType resType = types.top();

    switch (op) {
        case tAAND:
            addInsn(BC_IAAND);
            break;
        case tAOR:
            addInsn(BC_IAOR);
            break;
        case tAXOR:
            addInsn(BC_IAXOR);
            break;
        case tADD:
            if (resType == VT_INT)
                addInsn(BC_IADD);
            else
                addInsn(BC_DADD);
            break;
        case tSUB:
            if (resType == VT_INT)
                addInsn(BC_ISUB);
            else
                addInsn(BC_DSUB);
            break;
        case tMUL:
            if (resType == VT_INT)
                addInsn(BC_IMUL);
            else
                addInsn(BC_DMUL);
            break;
        case tDIV:
            if (resType == VT_INT)
                addInsn(BC_IDIV);
            else
                addInsn(BC_DDIV);
            break;
        case tMOD:
            addInsn(BC_IMOD);
            break;
        default:
            fprintf(stderr, "operator '%s' is not a valid binary math operator\n", tokenOp(op));
            assert(false);
    }
}

void BytecodeVisitor::binaryCompareOp(BinaryOpNode *node)
{
    node->left()->visit(this);
    node->right()->visit(this);

    TokenKind op = node->kind();

    correctTypes(2, opResType(op));
    VarType operandsType = types.top();

    if (operandsType == VT_STRING) {
        addInsn(BC_S2I);
        addInsn(BC_SWAP);
        addInsn(BC_S2I);
        addInsn(BC_SWAP);
        operandsType = VT_INT;
    }

    // TODO: can eliminate this for integers (performance)
    if (operandsType == VT_INT)
        addInsn(BC_ICMP);
    else
        addInsn(BC_DCMP);
    addInsn(BC_ILOAD0);

    Label thn(_fun->bytecode());
    Label els(_fun->bytecode());

    switch (op) {
        case tEQ:
            addBranch(BC_IFICMPE,  thn);
            break;
        case tNEQ:
            addBranch(BC_IFICMPNE, thn);
            break;
        case tGT:
            addBranch(BC_IFICMPG, thn);
            break;
        case tGE:
            addBranch(BC_IFICMPGE, thn);
            break;
        case tLT:
            addBranch(BC_IFICMPL, thn);
            break;
        case tLE:
            addBranch(BC_IFICMPLE, thn);
            break;

        default:
            fprintf(stderr, "operation %s is not a compare operation\n", tokenOp(op));
            assert(false);
    }

    addInsn(BC_ILOAD0);
    addBranch(BC_JA, els);
    _fun->bytecode()->bind(thn);

    types.pop();

    addInsn(BC_ILOAD1);
    _fun->bytecode()->bind(els);
}

void BytecodeVisitor::binaryLogicOp(BinaryOpNode *node)
{
    TokenKind op = node->kind();
    if (op == tOR) {
        Label fail(_fun->bytecode());
        Label succ(_fun->bytecode());

        node->left()->visit(this);
        VarType lhs = types.top();
        if (lhs == VT_STRING) {
            addInsn(BC_S2I);
            lhs = VT_INT;
        }

        if (lhs == VT_INT) {
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPNE, succ);
        } else {
            addInsn(BC_DLOAD0);
            addInsn(BC_DCMP);
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPNE, succ);
        }

        node->right()->visit(this);
        VarType rhs = types.top();
        if (rhs == VT_STRING) {
            addInsn(BC_S2I);
            rhs = VT_INT;
        }

        if (rhs == VT_INT) {
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPNE, succ);
        } else {
            addInsn(BC_DLOAD0);
            addInsn(BC_DCMP);
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPNE, succ);
        }

        addInsn(BC_ILOAD0);
        addBranch(BC_JA, fail);
        _fun->bytecode()->bind(succ);

        types.pop();

        addInsn(BC_ILOAD1);
        _fun->bytecode()->bind(fail);

    } else {
        Label fail(_fun->bytecode());
        Label succ(_fun->bytecode());

        node->left()->visit(this);
        VarType lhs = types.top();
        if (lhs == VT_STRING) {
            addInsn(BC_S2I);
            lhs = VT_INT;
        }

        if (lhs == VT_INT) {
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPE, fail);
        } else {
            addInsn(BC_DLOAD0);
            addInsn(BC_DCMP);
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPE, fail);
        }

        node->right()->visit(this);
        VarType rhs = types.top();
        if (rhs == VT_STRING) {
            addInsn(BC_S2I);
            rhs = VT_INT;
        }

        if (rhs == VT_INT) {
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPE, fail);
        } else {
            addInsn(BC_DLOAD0);
            addInsn(BC_DCMP);
            addInsn(BC_ILOAD0);
            addBranch(BC_IFICMPE, fail);
        }

        addInsn(BC_ILOAD1);
        addBranch(BC_JA, succ);
        _fun->bytecode()->bind(fail);

        types.pop();

        addInsn(BC_ILOAD0);
        _fun->bytecode()->bind(succ);
    }
}

void BytecodeVisitor::visitBinaryOpNode(BinaryOpNode *node)
{
//    fprintf(stderr, "visitBinaryOpNode '%s'\n", tokenOp(node->kind()));

    TokenKind op = node->kind();
    switch (op) {
        case tRANGE:
            node->left()->visit(this);
            node->right()->visit(this);
            correctTypes(2, opResType(op));
            break;
        case tEQ:
        case tNEQ:
        case tGT:
        case tGE:
        case tLT:
        case tLE:
            binaryCompareOp(node);
            break;
        case tOR:
        case tAND:
            binaryLogicOp(node);
            break;
        default:
            binaryMathOp(node);
            break;
    }
}

void BytecodeVisitor::visitUnaryOpNode(UnaryOpNode *node)
{
//    fprintf(stderr, "visitUnaryOpNode '%s'\n", tokenOp(node->kind()));

    node->operand()->visit(this);

    TokenKind op = node->kind();
    VarType resType = types.top();

    if (resType == VT_STRING) {
        addInsn(BC_S2I);
        resType = VT_INT;
    }

    switch (op) {
        case tADD:
            break;
        case tSUB:
            if (resType == VT_INT)
                addInsn(BC_INEG);
            else
                addInsn(BC_DNEG);
            break;
        case tNOT:
            if (resType == VT_INT) {
                addInsn(BC_ILOAD0);
                addInsn(BC_ICMP);
            } else {
                addInsn(BC_DLOAD0);
                addInsn(BC_DCMP);
            }
            addInsn(BC_ILOAD0);
            {
                Label l0(_fun->bytecode());
                Label l1(_fun->bytecode());

                addBranch(BC_IFICMPE, l1);
                addInsn(BC_ILOAD0);
                addBranch(BC_JA, l0);
                types.pop();
                _fun->bytecode()->bind(l1);
                addInsn(BC_ILOAD1);
                _fun->bytecode()->bind(l0);
            }
            break;
        default:
            fprintf(stderr, "operating %s is not an unary operation\n", tokenOp(op));
            assert(false);
    }
}

void BytecodeVisitor::visitStringLiteralNode(StringLiteralNode *node)
{
//    fprintf(stderr, "visitStringLiteralNode '%s'\n", node->literal().c_str());

    uint16_t id = _code->makeStringConstant(node->literal());
    addInsn(BC_SLOAD);
    _fun->bytecode()->addUInt16(id);
}

void BytecodeVisitor::visitIntLiteralNode(IntLiteralNode *node)
{
//    fprintf(stderr, "visitIntLiteralNode '%ld'\n", node->literal());

    addInsn(BC_ILOAD);
    _fun->bytecode()->addInt64(node->literal());
}

void BytecodeVisitor::visitDoubleLiteralNode(DoubleLiteralNode *node)
{
//    fprintf(stderr, "visitDoubleLiteralNode '%f'\n", node->literal());

    addInsn(BC_DLOAD);
    _fun->bytecode()->addDouble(node->literal());
}

void BytecodeVisitor::visitLoadNode(LoadNode *node)
{
//    fprintf(stderr, "visitLoadNode var '%s'\n", node->var()->name().c_str());

    const AstVar *var = node->var();
    uint16_t scope_id = _scope_map[var->owner()];
    uint16_t var_id = _var_map[var->owner()][var->name()];

//    fprintf(stderr, "scope_id %u, var_id %u, var name %s\n", scope_id, var_id, var->name().c_str());

    if (var->type() == VT_INT)
        addInsn(BC_LOADCTXIVAR);
    else if (var->type() == VT_DOUBLE)
        addInsn(BC_LOADCTXDVAR);
    else if (var->type() == VT_STRING)
        addInsn(BC_LOADCTXSVAR);

    _fun->bytecode()->addUInt16(scope_id);
    _fun->bytecode()->addUInt16(var_id);
}

void BytecodeVisitor::visitStoreNode(StoreNode *node)
{
//    fprintf(stderr, "visitStoreNode var '%s'\n", node->var()->name().c_str());

    const AstVar *var = node->var();
    uint16_t scope_id = _scope_map[var->owner()];
    uint16_t var_id = _var_map[var->owner()][var->name()];

    if (node->op() == tINCRSET || node->op() == tDECRSET) {
        LoadNode n(0, var);
        n.visit(this);
    }

    node->value()->visit(this);
    convertType(var->type());

    // use STORE*VAR if var belongs to the current scope, STORECTX otherwise
    if (var->type() == VT_INT) {
        if (node->op() == tINCRSET)
            addInsn(BC_IADD);
        if (node->op() == tDECRSET)
            addInsn(BC_ISUB);
        addInsn(BC_STORECTXIVAR);
    } else if (var->type() == VT_DOUBLE) {
        if (node->op() == tINCRSET)
            addInsn(BC_DADD);
        if (node->op() == tDECRSET)
            addInsn(BC_DSUB);
        addInsn(BC_STORECTXDVAR);
    } else if (var->type() == VT_STRING) {
        if (node->op() == tINCRSET)
            assert(false);
        if (node->op() == tDECRSET)
            assert(false);
        addInsn(BC_STORECTXSVAR);
    }

    _fun->bytecode()->addUInt16(scope_id);
    _fun->bytecode()->addUInt16(var_id);
}

void BytecodeVisitor::enterScope()
{
    _scopeSizes.push(types.size());
}

void BytecodeVisitor::leaveScope()
{
    size_t curSize = types.size();
    size_t prevSize = _scopeSizes.top();

    assert(curSize >= prevSize);

    while (curSize != prevSize) {
        addInsn(BC_POP);
        curSize--;
    }

    _scopeSizes.pop();
}

void BytecodeVisitor::visitBlockNode(BlockNode *node)
{
    _scope = node->scope();

//    fprintf(stderr, "visiting block for scope %p, scope id = %d\n", _scope, _scope_map[_scope]);

    enterScope();

    for (int i = 0; i < (int)node->nodes(); i++) {
        AstNode *child = node->nodeAt(i);
        enterScope(); // hack
        child->visit(this);
        leaveScope(); // hack, pop unused values
        _scope = node->scope();
    }

    leaveScope();

//    fprintf(stderr, "leaving block for scope %p, scope id = %d\n", _scope, _scope_map[_scope]);
}

void BytecodeVisitor::visitNativeCallNode(NativeCallNode *node)
{
//    fprintf(stderr, "native call to '%s' with type '%s'\n", node->nativeName().c_str(), typeToName(std::get<0>(node->nativeSignature()[0])));

    void *address = dlsym(_dlHandler, node->nativeName().c_str());
    assert(address != NULL);
    uint16_t id = _code->makeNativeFunction(node->nativeName(), node->nativeSignature(), address);

    addInsn(BC_CALLNATIVE);
    _fun->bytecode()->addUInt16(id);
}

void BytecodeVisitor::visitForNode(ForNode *node)
{
    node->inExpr()->visit(this);
    addInsn(BC_SWAP);

    assert(node->var()->type() == VT_INT);

    uint16_t scope_id = _scope_map[node->var()->owner()];
    uint16_t var_id = _var_map[node->var()->owner()][node->var()->name()];

    Label begin = _fun->bytecode()->currentLabel();

    addInsn(BC_STORECTXIVAR);
    _fun->bytecode()->addUInt16(scope_id);
    _fun->bytecode()->addUInt16(var_id);

    addInsn(BC_STOREIVAR1);
    addInsn(BC_LOADIVAR1);

    addInsn(BC_LOADCTXIVAR);
    _fun->bytecode()->addUInt16(scope_id);
    _fun->bytecode()->addUInt16(var_id);

    addInsn(BC_LOADIVAR1);

    Label done(_fun->bytecode());
    addBranch(BC_IFICMPG, done);

    node->body()->visit(this);

    addInsn(BC_LOADCTXIVAR);
    _fun->bytecode()->addUInt16(scope_id);
    _fun->bytecode()->addUInt16(var_id);
    addInsn(BC_ILOAD1);
    addInsn(BC_IADD);

    addBranch(BC_JA, begin);
    _fun->bytecode()->bind(done);

    types.pop();
    addInsn(BC_POP);
}

void BytecodeVisitor::visitWhileNode(WhileNode *node)
{
    Label repeat = _fun->bytecode()->currentLabel();

    node->whileExpr()->visit(this);

    VarType expType = types.top();
    if (expType == VT_DOUBLE)
        addInsn(BC_D2I);
    else if (expType == VT_STRING)
        addInsn(BC_S2I);

    addInsn(BC_ILOAD0);

    Label done(_fun->bytecode());

    addBranch(BC_IFICMPE, done);

    node->loopBlock()->visit(this);

    addBranch(BC_JA, repeat);
    _fun->bytecode()->bind(done);
}

void BytecodeVisitor::visitIfNode(IfNode *node)
{
    node->ifExpr()->visit(this);

    VarType expType = types.top();
    if (expType == VT_DOUBLE)
        addInsn(BC_D2I);
    else if (expType == VT_STRING)
        addInsn(BC_S2I);

    addInsn(BC_ILOAD0);

    Label notThen(_fun->bytecode());
    Label notElse(_fun->bytecode());

    addBranch(BC_IFICMPE, notThen);

    node->thenBlock()->visit(this);

    if (node->elseBlock())
        addBranch(BC_JA, notElse);

    _fun->bytecode()->bind(notThen);

    if (node->elseBlock()) {
        node->elseBlock()->visit(this);

        _fun->bytecode()->bind(notElse);
    }
}

void BytecodeVisitor::visitReturnNode(ReturnNode *node)
{
//    fprintf(stderr, "visitReturnNode\n");

    if (node->returnExpr()) {
        node->returnExpr()->visit(this);
        convertType(_fun->returnType());
    }

/**
 * Really?
 * int a() native 'a' -> { native call a(); return; }
 * wtf???
 */

    AstFunction *a_fun = _funcs[_funIdMap[_fun->name()]];

    if (!isNative(a_fun->node())) {
        VarType returnType = _fun->returnType();
        if (returnType == VT_INT)
            addInsn(BC_STOREIVAR0);
        else if (returnType == VT_DOUBLE)
            addInsn(BC_STOREDVAR0);
        else if (returnType == VT_STRING)
            addInsn(BC_STORESVAR0);
    }

    addInsn(BC_RETURN);
}

void BytecodeVisitor::visitFunctionNode(FunctionNode *node)
{
//    fprintf(stderr, "visiting function node name %s\n", node->name().c_str());
//    fprintf(stderr, "scope id is %u\n", _scope_map[_scope]);

    // args
    for (int i = node->parametersNumber() - 1; i >= 0; i--) {
        VarType paramType = node->parameterType(i);
        if (paramType == VT_INT)
            types.push(VT_INT);
        else if (paramType == VT_DOUBLE)
            types.push(VT_DOUBLE);
        else if (paramType == VT_STRING)
            types.push(VT_STRING);
        else
            assert(false);
    }

    if (!isNative(node)) {
        for (size_t i = 0; i < node->parametersNumber(); i++) {
            VarType paramType = node->parameterType(i);
            if (paramType == VT_INT)
                addInsn(BC_STORECTXIVAR);
            else if (paramType == VT_DOUBLE)
                addInsn(BC_STORECTXDVAR);
            else if (paramType == VT_STRING)
                addInsn(BC_STORECTXSVAR);
//            fprintf(stderr, "param: scopeId = %u, varId = %u, name = '%s'\n", _scope_map[_scope], _var_map[_scope][node->parameterName(i)], node->parameterName(i).c_str());
            _fun->bytecode()->addUInt16(_scope_map[_scope]);
            _fun->bytecode()->addUInt16(_var_map[_scope][node->parameterName(i)]);
        }
    }

    node->body()->visit(this);

    if (!isNative(node)) {
        if (_fun->bytecode()->getInsn(_fun->bytecode()->current() - 1) != BC_RETURN)
            addInsn(BC_RETURN); // fake return just in case
    } else {
        while (types.size() != 0)
            types.pop();
    }
}

void BytecodeVisitor::visitCallNode(CallNode *node)
{
    AstFunction *fun = _funcs[_funIdMap[node->name()]];
    assert(node->parametersNumber() == fun->parametersNumber());

    for (int i = node->parametersNumber() - 1; i >= 0; i--) {
        node->parameterAt(i)->visit(this);
        convertType(fun->parameterType(i));
    }

    addInsn(BC_CALL);
    _fun->bytecode()->addUInt16(_funIdMap[node->name()]);

    for (int i = node->parametersNumber() - 1; i >= 0; i--)
        types.pop();

    VarType funType = fun->returnType();
    if (funType == VT_INT)
        addInsn(BC_LOADIVAR0);
    else if (funType == VT_DOUBLE)
        addInsn(BC_LOADDVAR0);
    else if (funType == VT_STRING)
        addInsn(BC_LOADSVAR0);
}

void BytecodeVisitor::visitPrintNode(PrintNode *node)
{
    for (int i = 0; i < (int)node->operands(); i++) {
        node->operandAt(i)->visit(this);
        VarType opType = types.top();
        if (opType == VT_INT)
            addInsn(BC_IPRINT);
        else if (opType == VT_DOUBLE)
            addInsn(BC_DPRINT);
        else if (opType == VT_STRING)
            addInsn(BC_SPRINT);
        else
            assert(false);
    }
}
