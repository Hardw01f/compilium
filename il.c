#include "compilium.h"

#define REG_NULL 0

typedef struct CONTEXT Context;
struct CONTEXT {
  const Context *parent;
  ASTDict *dict;
};

Context *AllocContext(const Context *parent) {
  Context *context = malloc(sizeof(Context));
  context->parent = parent;
  context->dict = AllocASTDict(8);
  return context;
}

ASTNode *FindIdentInContext(const Context *context, ASTIdent *ident) {
  if (!context) return NULL;
  ASTNode *result = FindASTNodeInDict(context->dict, ident->token->str);
  if (result) return result;
  return FindIdentInContext(context->parent, ident);
}

void AppendLocalVarInContext(Context *context, const Token *token) {
  int ofs = GetSizeOfASTDict(context->dict) + 1;
  printf("LocalVar[%d]: %s\n", ofs, token->str);
  AppendASTNodeToDict(context->dict, token->str,
                      ToASTNode(AllocASTLocalVar(ofs)));
}

const char *ILOpTypeName[kNumOfILOpFunc];

void InitILOpTypeName() {
  ILOpTypeName[kILOpAdd] = "Add";
  ILOpTypeName[kILOpSub] = "Sub";
  ILOpTypeName[kILOpMul] = "Mul";
  ILOpTypeName[kILOpDiv] = "Div";
  ILOpTypeName[kILOpMod] = "Mod";
  ILOpTypeName[kILOpLoadImm] = "LoadImm";
  ILOpTypeName[kILOpLoadIdent] = "LoadIdent";
  ILOpTypeName[kILOpFuncBegin] = "FuncBegin";
  ILOpTypeName[kILOpFuncEnd] = "FuncEnd";
  ILOpTypeName[kILOpReturn] = "Return";
  ILOpTypeName[kILOpCall] = "Call";
  ILOpTypeName[kILOpWriteLocalVar] = "WriteLocalVar";
  ILOpTypeName[kILOpReadLocalVar] = "ReadLocalVar";
}

const char *GetILOpTypeName(ILOpType type) {
  if (kNumOfILOpFunc <= type) return "?";
  return ILOpTypeName[type];
}

int GetRegNumber() {
  static int num = 2;
  return num++;
}

// generators
ASTILOp *GenerateILFor(ASTList *il, ASTNode *node, Context *context);

void GenerateILForCompStmt(ASTList *il, ASTNode *node, Context *context) {
  ASTCompStmt *comp = ToASTCompStmt(node);
  ASTList *stmt_list = comp->stmt_list;
  for (int i = 0; i < GetSizeOfASTList(stmt_list); i++) {
    GenerateILFor(il, GetASTNodeAt(stmt_list, i), context);
  }
}

void GenerateILForFuncDef(ASTList *il, ASTNode *node, Context *context) {
  ASTFuncDef *def = ToASTFuncDef(node);
  PushASTNodeToList(
      il, ToASTNode(AllocAndInitASTILOp(kILOpFuncBegin, REG_NULL, REG_NULL,
                                        REG_NULL, node)));
  GenerateILForCompStmt(il, ToASTNode(def->comp_stmt), AllocContext(context));
  PushASTNodeToList(il, ToASTNode(AllocAndInitASTILOp(
                            kILOpFuncEnd, REG_NULL, REG_NULL, REG_NULL, node)));
}

ASTILOp *GenerateILForExprBinOp(ASTList *il, ASTNode *node, Context *context) {
  int dst = REG_NULL;
  ASTExprBinOp *bin_op = ToASTExprBinOp(node);
  ILOpType il_op_type = kILOpNop;
  if (IsEqualToken(bin_op->op, "+")) {
    il_op_type = kILOpAdd;
  } else if (IsEqualToken(bin_op->op, "-")) {
    il_op_type = kILOpSub;
  } else if (IsEqualToken(bin_op->op, "*")) {
    il_op_type = kILOpMul;
  } else if (IsEqualToken(bin_op->op, "/")) {
    il_op_type = kILOpDiv;
  } else if (IsEqualToken(bin_op->op, "%")) {
    il_op_type = kILOpMod;
  }
  if (il_op_type != kILOpNop) {
    dst = GetRegNumber();
    int il_left = GenerateILFor(il, bin_op->left, context)->dst_reg;
    int il_right = GenerateILFor(il, bin_op->right, context)->dst_reg;
    ASTILOp *il_op =
        AllocAndInitASTILOp(il_op_type, dst, il_left, il_right, node);
    PushASTNodeToList(il, ToASTNode(il_op));
    return il_op;
  } else if (IsEqualToken(bin_op->op, "=")) {
    ASTIdent *left_ident = ToASTIdent(bin_op->left);
    if (left_ident) {
      ASTNode *var = FindIdentInContext(context, left_ident);
      ASTLocalVar *local_var = ToASTLocalVar(var);
      if (local_var) {
        int il_right = GenerateILFor(il, bin_op->right, context)->dst_reg;
        ASTILOp *il_op =
            AllocAndInitASTILOp(kILOpWriteLocalVar, il_right, REG_NULL,
                                il_right, ToASTNode(local_var));
        PushASTNodeToList(il, ToASTNode(il_op));
        return il_op;
      }
      Error("local variable %s not defined here.", left_ident->token->str);
    }
    Error("Left operand of assignment should be an lvalue");
  } else if (IsEqualToken(bin_op->op, ",")) {
    GenerateILFor(il, bin_op->left, context);
    return GenerateILFor(il, bin_op->right, context);
  } else if (IsEqualToken(bin_op->op, "(")) {
    // func_call
    // call_params = [func_addr: ILOp, arg1: ILOp, arg2: ILOp, ...]
    ASTList *call_params = AllocASTList(8);

    // func_addr
    if (bin_op->left->type == kASTIdent) {
      PushASTNodeToList(call_params, bin_op->left);
    } else {
      Error("Calling non-labeled function is not implemented.");
    }

    // args
    if (bin_op->right) {
      ASTList *arg_list = ToASTList(bin_op->right);
      if (!arg_list) Error("arg_list is not an ASTList");
      for (int i = 0; i < GetSizeOfASTList(arg_list); i++) {
        ASTNode *node = GetASTNodeAt(arg_list, i);
        PushASTNodeToList(call_params,
                          ToASTNode(GenerateILFor(il, node, context)));
      }
    }
    ASTILOp *il_op_call = AllocAndInitASTILOp(
        kILOpCall, GetRegNumber(), REG_NULL, REG_NULL, ToASTNode(call_params));
    PushASTNodeToList(il, ToASTNode(il_op_call));
    return il_op_call;
  }
  Error("Not implemented GenerateILForExprBinOp (op: %s)", bin_op->op->str);
  return NULL;
}

ASTILOp *GenerateILForConstant(ASTList *il, ASTNode *node, Context *context) {
  int dst = GetRegNumber();
  ASTILOp *il_op =
      AllocAndInitASTILOp(kILOpLoadImm, dst, REG_NULL, REG_NULL, node);
  PushASTNodeToList(il, ToASTNode(il_op));
  return il_op;
}

ASTILOp *GenerateILForIdent(ASTList *il, ASTNode *node, Context *context) {
  int dst = GetRegNumber();
  ASTNode *var = FindIdentInContext(context, ToASTIdent(node));
  ASTLocalVar *local_var = ToASTLocalVar(var);
  ASTILOp *il_op =
      local_var
          ? AllocAndInitASTILOp(kILOpReadLocalVar, dst, REG_NULL, REG_NULL,
                                ToASTNode(local_var))
          : AllocAndInitASTILOp(kILOpLoadIdent, dst, REG_NULL, REG_NULL, node);
  PushASTNodeToList(il, ToASTNode(il_op));
  return il_op;
}

ASTILOp *GenerateILForExprStmt(ASTList *il, ASTNode *node, Context *context) {
  const ASTExprStmt *expr_stmt = ToASTExprStmt(node);
  return GenerateILFor(il, expr_stmt->expr, context);
}

ASTILOp *GenerateILForJumpStmt(ASTList *il, ASTNode *node, Context *context) {
  ASTJumpStmt *jump_stmt = ToASTJumpStmt(node);
  if (IsEqualToken(jump_stmt->kw->token, "return")) {
    int expr_reg =
        GenerateILForExprStmt(il, jump_stmt->param, context)->dst_reg;

    ASTILOp *il_op =
        AllocAndInitASTILOp(kILOpReturn, REG_NULL, expr_reg, REG_NULL, node);
    PushASTNodeToList(il, ToASTNode(il_op));
    return il_op;
  }
  Error("Not implemented JumpStmt (%s)", jump_stmt->kw->token->str);
  return NULL;
}

void GenerateILForDecl(ASTList *il, ASTNode *node, Context *context) {
  ASTDecl *decl = ToASTDecl(node);
  if (!decl) Error("node is not a Decl");
  PrintASTNode(ToASTNode(decl->decl_specs), 0);
  putchar('\n');
  for (int i = 0; i < GetSizeOfASTList(decl->init_decltors); i++) {
    ASTDecltor *decltor = ToASTDecltor(GetASTNodeAt(decl->init_decltors, i));
    AppendLocalVarInContext(context, GetIdentTokenFromDecltor(decltor));
  }
}

ASTILOp *GenerateILFor(ASTList *il, ASTNode *node, Context *context) {
  printf("GenerateIL: AST%s...\n", GetASTTypeName(node));
  if (node->type == kASTList) {
    // translation-unit
    ASTList *list = ToASTList(node);
    for (int i = 0; i < GetSizeOfASTList(list); i++) {
      ASTNode *child_node = GetASTNodeAt(list, i);
      if (child_node->type == kASTFuncDef) {
        GenerateILForFuncDef(il, child_node, context);
      }
    }
    return NULL;
  } else if (node->type == kASTJumpStmt) {
    return GenerateILForJumpStmt(il, node, context);
  } else if (node->type == kASTExprBinOp) {
    return GenerateILForExprBinOp(il, node, context);
  } else if (node->type == kASTConstant) {
    return GenerateILForConstant(il, node, context);
  } else if (node->type == kASTExprStmt) {
    return GenerateILForExprStmt(il, node, context);
  } else if (node->type == kASTIdent) {
    return GenerateILForIdent(il, node, context);
  } else if (node->type == kASTDecl) {
    GenerateILForDecl(il, node, context);
    return NULL;
  }
  PrintASTNode(node, 0);
  putchar('\n');
  Error("IL Generation for AST%s is not implemented.", GetASTTypeName(node));
  return NULL;
}

#define MAX_IL_NODES 2048
ASTList *GenerateIL(ASTNode *root) {
  ASTList *il = AllocASTList(MAX_IL_NODES);
  Context *context = AllocContext(NULL);
  GenerateILFor(il, root, context);
  return il;
}