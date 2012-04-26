#include "Compiler.h"
#include "Method.h"
#include "Node.h"
#include "Object.h"
#include "VM.h"

namespace magpie
{
  bool Compiler::compileModule(VM& vm, gc<ModuleAst> module)
  {
    // Declare methods first so we can resolve mutually recursive calls.
    for (int i = 0; i < module->methods().count(); i++)
    {
      const MethodAst& methodAst = *module->methods()[i];
      vm.methods().declare(methodAst.name());
    }
    
    // Try to compile all of the methods.
    for (int i = 0; i < module->methods().count(); i++)
    {
      const MethodAst& methodAst = *module->methods()[i];
      temp<Method> method = compileMethod(vm, methodAst);

      // Bail if there was a compile error.
      if (method.isNull()) return false;
      
      vm.methods().define(methodAst.name(), method);
    }
    
    return true;
  }
  
  temp<Method> Compiler::compileMethod(VM& vm, const MethodAst& methodAst)
  {
    Compiler compiler(vm);
    temp<Method> method = compiler.compile(methodAst);
    if (compiler.hasError_) return temp<Method>();
    return method;
  }
  
  Compiler::Compiler(VM& vm)
  : NodeVisitor(),
    vm_(vm),
    locals_(),
    code_(),
    constants_(),
    numInUseRegisters_(0),
    variableStart_(0),
    maxRegisters_(0),
    hasError_(false)
  {}

  temp<Method> Compiler::compile(const MethodAst& method)
  {
    // Create a register for the argument and result value.
    int result = allocateRegister();

    // Add a fake local for it so that local slots line up with their registers.
    locals_.add(String::create("(return)"));
    
    // TODO(bob): Hackish and temporary.
    if (!method.parameter().isNull())
    {
      // Evaluate the method's parameter pattern.
      reserveVariables(method.parameter()->countVariables());
      method.parameter()->accept(*this, result);
    }
    
    method.body().accept(*this, result);
    write(OP_END, result);
    
    return Method::create(method.name(), code_, constants_, maxRegisters_);
  }
  
  void Compiler::visit(const BinaryOpNode& node, int dest)
  {
    switch (node.type())
    {
      case TOKEN_PLUS:      compileInfix(node, OP_ADD, dest); break;
      case TOKEN_MINUS:     compileInfix(node, OP_SUBTRACT, dest); break;
      case TOKEN_STAR:      compileInfix(node, OP_MULTIPLY, dest); break;
      case TOKEN_SLASH:     compileInfix(node, OP_DIVIDE, dest); break;
      case TOKEN_LESS_THAN: compileInfix(node, OP_LESS_THAN, dest); break;
        
      default:
        ASSERT(false, "Unknown infix operator.");
    }
  }
  
  void Compiler::visit(const BoolNode& node, int dest)
  {
    write(OP_BOOL, node.value() ? 1 : 0, dest);
  }
  
  void Compiler::visit(const CallNode& node, int dest)
  {
    int method = vm_.methods().find(node.name());
    
    if (method == -1)
    {
      error("Method '%s' is not defined.", node.name()->cString());
      return;
    }
    
    ASSERT(node.leftArg().isNull(), "Left-hand arguments aren't supported yet.");
    
    // Compile the argument.
    node.rightArg()->accept(*this, dest);
    
    write(OP_CALL, method, dest);
  }
  
  void Compiler::visit(const IfNode& node, int dest)
  {
    // Compile the condition.
    node.condition().accept(*this, dest);
    
    // Leave a space for the test and jump instruction.
    int jumpToElse = startJump();
    
    // Compile the then arm.
    node.thenArm().accept(*this, dest);
    
    // Leave a space for the then arm to jump over the else arm.
    int jumpPastElse = startJump();
    
    // Compile the else arm.
    endJump(jumpToElse, OP_JUMP_IF_FALSE, dest, code_.count() - jumpToElse - 1);
      
    node.elseArm().accept(*this, dest);
      
    endJump(jumpPastElse, OP_JUMP, code_.count() - jumpPastElse - 1);
  }
  
  void Compiler::visit(const NameNode& node, int dest)
  {
    int local = locals_.indexOf(node.name());
    
    if (local == -1)
    {
      error("Variable '%s' is not defined.", node.name()->cString());
      return;
    }
    
    write(OP_MOVE, local, dest);
  }
  
  void Compiler::visit(const NumberNode& node, int dest)
  {
    int index = compileConstant(node);
    write(OP_CONSTANT, index, dest);
  }
  
  void Compiler::visit(const SequenceNode& node, int dest)
  {
    for (int i = 0; i < node.expressions().count(); i++)
    {
      // TODO(bob): Could compile all but the last expression with a special
      // sigil dest that means "won't use" and some nodes could check that to
      // omit some unnecessary instructions.
      node.expressions()[i]->accept(*this, dest);
    }
  }
  
  void Compiler::visit(const StringNode& node, int dest)
  {
    int index = compileConstant(node);
    write(OP_CONSTANT, index, dest);
  }
  
  void Compiler::visit(const VariableNode& node, int dest)
  {
    // Reserve the registers up front. This way we'll compile the value to
    // a slot *after* them. We want registers to come before temporaries
    // because they don't have nice stack-like semantics like temporary
    // registers do.
    reserveVariables(node.pattern()->countVariables());
    
    // Compile the value.
    int valueReg = allocateRegister();
    node.value()->accept(*this, valueReg);
    
    // TODO(bob): Handle mutable variables.
    
    // Now pattern match on it.
    node.pattern()->accept(*this, valueReg);
    
    // Copy the final result.
    // TODO(bob): Omit this in cases where it won't be used. Most variable
    // declarations are just in sequences.
    write(OP_MOVE, valueReg, dest);
    
    releaseRegister(); // valueReg.
  }
  
  void Compiler::visit(const VariablePattern& pattern, int value)
  {
    // Declare the variable.
    locals_.add(pattern.name());
    int variable = allocateVariable();
    ASSERT(locals_.count() - 1 == variable,
           "Locals array needs to be in sync with registers for locals.");
    
    // Copy the value into the new variable.
    write(OP_MOVE, value, variable);
  }

  void Compiler::compileInfix(const BinaryOpNode& node, OpCode op, int dest)
  {
    int a = compileExpressionOrConstant(node.left());
    int b = compileExpressionOrConstant(node.right());
    
    write(op, a, b, dest);
    
    if (IS_REGISTER(a)) releaseRegister();
    if (IS_REGISTER(b)) releaseRegister();
  }
  
  int Compiler::compileExpressionOrConstant(const Node& node)
  {
    const NumberNode* number = node.asNumberNode();
    if (number != NULL)
    {
      return MAKE_CONSTANT(compileConstant(*number));
    }
    
    const StringNode* string = node.asStringNode();
    if (string != NULL)
    {
      return MAKE_CONSTANT(compileConstant(*string));
    }
    
    int dest = allocateRegister();
      
    node.accept(*this, dest);
    return dest;
  }
  
  int Compiler::compileConstant(const NumberNode& node)
  {
    temp<NumberObject> constant = Object::create(node.value());
    
    // TODO(bob): Should check for duplicates. Only need one copy of any
    // given constant.
    constants_.add(constant);
    return constants_.count() - 1;
  }
  
  int Compiler::compileConstant(const StringNode& node)
  {
    temp<StringObject> constant = Object::create(node.value());
    
    // TODO(bob): Should check for duplicates. Only need one copy of any
    // given constant.
    constants_.add(constant);
    return constants_.count() - 1;
  }
  
  void Compiler::write(OpCode op, int a, int b, int c)
  {
    ASSERT_INDEX(a, 256);
    ASSERT_INDEX(b, 256);
    ASSERT_INDEX(c, 256);
    
    code_.add(MAKE_ABC(a, b, c, op));
  }

  int Compiler::startJump()
  {
    // Just write a dummy op to leave a space for the jump instruction.
    write(OP_MOVE);
    return code_.count() - 1;
  }
  
  void Compiler::endJump(int from, OpCode op, int a, int b, int c)
  {
    code_[from] = MAKE_ABC(a, b, c, op);
  }

  int Compiler::allocateRegister()
  {
    numInUseRegisters_++;
    
    if (maxRegisters_ < numInUseRegisters_)
    {
      maxRegisters_ = numInUseRegisters_;
    }
    
    return numInUseRegisters_ - 1;
  }
  
  void Compiler::releaseRegister()
  {
    ASSERT(numInUseRegisters_ > 0, "Released too many registers.");
    numInUseRegisters_--;
  }
  
  void Compiler::reserveVariables(int count)
  {
    variableStart_ = numInUseRegisters_;
    
    // Reserve the registers for them.
    numInUseRegisters_++;
    if (maxRegisters_ < numInUseRegisters_)
    {
      maxRegisters_ = numInUseRegisters_;
    }
  }

  int Compiler::allocateVariable()
  {
    ASSERT(variableStart_ < numInUseRegisters_,
        "Cannot allocate an unreserved variable.");
    
    return variableStart_++;
  }
  
  void Compiler::error(const char* format, ...)
  {
    hasError_ = true;
    // TODO(bob): Hackish. Need to figure out if we want C-style, C++-style or
    // Magpie GC strings for compiler error messages.
    char result[512];
    
    va_list args;
    va_start (args, format);
    
    vsprintf(result, format, args);
    
    va_end (args);
    
    std::cout << "Error: " << result << std::endl;
  }
}