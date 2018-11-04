#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum OSType {
  kOSDarwin,
  kOSLinux,
};

struct CompilerArgs {
  const char *input;
};

const char *symbol_prefix;

#define assert(expr) \
  ((void)((expr) || (__assert(#expr, __FILE__, __LINE__), 0)))

enum TokenTypes {
  kTokenDecimalNumber,
  kTokenOctalNumber,
  kTokenPlus,
  kTokenStar,
  kTokenMinus,
  kTokenSlash,
  kTokenPercent,
  kTokenShiftLeft,
  kTokenShiftRight,
  kNumOfTokenTypeNames
};

struct Token {
  const char *begin;
  int length;
  enum TokenTypes type;
  const char *src_str;
};

#define NUM_OF_TOKENS 32
struct Token tokens[NUM_OF_TOKENS];
int tokens_used;

_Noreturn void Error(const char *fmt, ...) {
  fflush(stdout);
  fprintf(stderr, "Error: ");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}

void __assert(const char *expr_str, const char *file, int line) {
  Error("Assertion failed: %s at %s:%d\n", expr_str, file, line);
}

const char *token_type_names[kNumOfTokenTypeNames];

void ParseCompilerArgs(struct CompilerArgs *args, int argc, char **argv) {
  args->input = NULL;
  symbol_prefix = "_";
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--target_os") == 0) {
      i++;
      if (strcmp(argv[i], "Darwin") == 0) {
        symbol_prefix = "_";
      } else if (strcmp(argv[i], "Linux") == 0) {
        symbol_prefix = "";
      } else {
        Error("Unknown os type %s", argv[i]);
      }
    } else {
      args->input = argv[i];
    }
  }
  if (!args->input)
    Error("Usage: %s [--os_type=Linux|Darwin] src_string", argv[0]);
}

void InitTokenTypeNames() {
  token_type_names[kTokenDecimalNumber] = "DecimalNumber";
  token_type_names[kTokenOctalNumber] = "OctalNumber";
  token_type_names[kTokenPlus] = "Plus";
  token_type_names[kTokenStar] = "Star";
  token_type_names[kTokenMinus] = "Minus";
  token_type_names[kTokenSlash] = "Slash";
  token_type_names[kTokenPercent] = "Percent";
  token_type_names[kTokenShiftLeft] = "ShiftLeft";
  token_type_names[kTokenShiftRight] = "ShiftRight";
}

const char *GetTokenTypeName(const struct Token *t) {
  assert(t && 0 <= t->type && t->type < kNumOfTokenTypeNames);
  assert(token_type_names[t->type]);
  return token_type_names[t->type];
}

void AddToken(const char *src_str, const char *begin, int length,
              enum TokenTypes type) {
  assert(tokens_used < NUM_OF_TOKENS);
  tokens[tokens_used].begin = begin;
  tokens[tokens_used].length = length;
  tokens[tokens_used].type = type;
  tokens[tokens_used].src_str = src_str;
  tokens_used++;
}

void Tokenize(const char *src) {
  const char *s = src;
  while (*s) {
    if ('1' <= *s && *s <= '9') {
      int length = 0;
      while ('0' <= s[length] && s[length] <= '9') {
        length++;
      }
      AddToken(src, s, length, kTokenDecimalNumber);
      s += length;
      continue;
    }
    if ('0' == *s) {
      int length = 0;
      while ('0' <= s[length] && s[length] <= '7') {
        length++;
      }
      AddToken(src, s, length, kTokenOctalNumber);
      s += length;
      continue;
    }
    if ('+' == *s) {
      AddToken(src, s++, 1, kTokenPlus);
      continue;
    }
    if ('-' == *s) {
      AddToken(src, s++, 1, kTokenMinus);
      continue;
    }
    if ('*' == *s) {
      AddToken(src, s++, 1, kTokenStar);
      continue;
    }
    if ('/' == *s) {
      AddToken(src, s++, 1, kTokenSlash);
      continue;
    }
    if ('%' == *s) {
      AddToken(src, s++, 1, kTokenPercent);
      continue;
    }
    if ('<' == *s) {
      if (s[1] == '<') {
        AddToken(src, s, 2, kTokenShiftLeft);
        s += 2;
        continue;
      }
    }
    if ('>' == *s) {
      if (s[1] == '>') {
        AddToken(src, s, 2, kTokenShiftRight);
        s += 2;
        continue;
      }
    }
    Error("Unexpected char %c", *s);
  }
}

void PrintToken(struct Token *t) {
  fprintf(stderr, "(Token %.*s type=%s)", t->length, t->begin,
          GetTokenTypeName(t));
}

void PrintTokens() {
  for (int i = 0; i < tokens_used; i++) {
    struct Token *t = &tokens[i];
    PrintToken(t);
    fputc('\n', stderr);
  }
}

int token_stream_index;
struct Token *ConsumeToken(enum TokenTypes type) {
  if (token_stream_index < tokens_used &&
      tokens[token_stream_index].type == type) {
    return &tokens[token_stream_index++];
  }
  return NULL;
}

struct Token *NextToken() {
  if (token_stream_index < tokens_used) {
    return &tokens[token_stream_index++];
  }
  return NULL;
}

_Noreturn void ErrorWithToken(struct Token *t, const char *fmt, ...) {
  assert(t);
  const char *line_begin = t->begin;
  while (t->src_str < line_begin) {
    if (line_begin[-1] == '\n') break;
    line_begin--;
  }

  for (const char *p = line_begin; *p && *p != '\n'; p++) {
    fputc(*p <= ' ' ? ' ' : *p, stderr);
  }
  fputc('\n', stderr);
  const char *p;
  for (p = line_begin; p < t->begin; p++) {
    fputc(' ', stderr);
  }
  for (int i = 0; i < t->length; i++) {
    fputc('^', stderr);
    p++;
  }
  for (; *p && *p != '\n'; p++) {
    fputc(' ', stderr);
  }
  fputc('\n', stderr);

  fflush(stdout);
  fprintf(stderr, "Error: ");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}

struct ASTNode {
  struct Token *op;
  int reg;
  struct ASTNode *left;
  struct ASTNode *right;
};

struct ASTNode *AllocASTNode() {
  return calloc(1, sizeof(struct ASTNode));
}

void PrintASTNode(struct ASTNode *n) {
  fprintf(stderr, "(");
  PrintToken(n->op);
  fprintf(stderr, " reg: %d", n->reg);
  if (n->left) {
    fprintf(stderr, " left: ");
    PrintASTNode(n->left);
  }
  if (n->right) {
    fprintf(stderr, " right: ");
    PrintASTNode(n->right);
  }
  fprintf(stderr, ")");
}

struct ASTNode *ParsePrimaryExpr() {
  struct Token *t;
  if ((t = ConsumeToken(kTokenDecimalNumber)) ||
      (t = ConsumeToken(kTokenOctalNumber))) {
    struct ASTNode *op = AllocASTNode();
    op->op = t;
    return op;
  }
  return NULL;
}

struct ASTNode *ParseMulExpr() {
  struct ASTNode *op = ParsePrimaryExpr();
  if (!op) return NULL;
  struct Token *t;
  while ((t = ConsumeToken(kTokenStar)) || (t = ConsumeToken(kTokenSlash)) ||
         (t = ConsumeToken(kTokenPercent))) {
    struct ASTNode *right = ParsePrimaryExpr();
    if (!right) ErrorWithToken(t, "Expected expression after binary operator");
    struct ASTNode *new_op = AllocASTNode();
    new_op->op = t;
    new_op->left = op;
    new_op->right = right;
    op = new_op;
  }
  return op;
}

struct ASTNode *ParseAddExpr() {
  struct ASTNode *op = ParseMulExpr();
  if (!op) return NULL;
  struct Token *t;
  while ((t = ConsumeToken(kTokenPlus)) || (t = ConsumeToken(kTokenMinus))) {
    struct ASTNode *right = ParseMulExpr();
    if (!right) ErrorWithToken(t, "Expected expression after binary operator");
    struct ASTNode *new_op = AllocASTNode();
    new_op->op = t;
    new_op->left = op;
    new_op->right = right;
    op = new_op;
  }
  return op;
}

struct ASTNode *ParseShiftExpr() {
  struct ASTNode *op = ParseAddExpr();
  if (!op) return NULL;
  struct Token *t;
  while ((t = ConsumeToken(kTokenShiftLeft)) ||
         (t = ConsumeToken(kTokenShiftRight))) {
    struct ASTNode *right = ParseAddExpr();
    if (!right) ErrorWithToken(t, "Expected expression after binary operator");
    struct ASTNode *new_op = AllocASTNode();
    new_op->op = t;
    new_op->left = op;
    new_op->right = right;
    op = new_op;
  }
  return op;
}

struct ASTNode *Parse() {
  struct ASTNode *ast = ParseShiftExpr();
  struct Token *t;
  if (!(t = NextToken())) return ast;
  ErrorWithToken(t, "Unexpected token");
}

#define NUM_OF_SCRATCH_REGS 4
const char *reg_names_64[NUM_OF_SCRATCH_REGS] = {"rdi", "rsi", "r8", "r9"};

int reg_used_table[NUM_OF_SCRATCH_REGS];

int AllocReg() {
  for (int i = 0; i < NUM_OF_SCRATCH_REGS; i++) {
    if (!reg_used_table[i]) {
      reg_used_table[i] = 1;
      return i;
    }
  }
  Error("No more regs");
}
void FreeReg(int reg) {
  assert(0 <= reg && reg < NUM_OF_SCRATCH_REGS);
  reg_used_table[reg] = 0;
}

void Generate(struct ASTNode *node) {
  assert(node && node->op);
  if (node->op->type == kTokenDecimalNumber ||
      node->op->type == kTokenOctalNumber) {
    node->reg = AllocReg();

    printf("mov %s, %ld\n", reg_names_64[node->reg],
           strtol(node->op->begin, NULL, 0));
    return;
  }
  if (node->op->type == kTokenPlus) {
    Generate(node->left);
    Generate(node->right);
    node->reg = node->left->reg;
    FreeReg(node->right->reg);

    printf("add %s, %s\n", reg_names_64[node->reg],
           reg_names_64[node->right->reg]);
    return;
  }
  if (node->op->type == kTokenMinus) {
    Generate(node->left);
    Generate(node->right);
    node->reg = node->left->reg;
    FreeReg(node->right->reg);

    printf("sub %s, %s\n", reg_names_64[node->reg],
           reg_names_64[node->right->reg]);
    return;
  }
  if (node->op->type == kTokenStar) {
    Generate(node->left);
    Generate(node->right);
    node->reg = node->left->reg;
    FreeReg(node->right->reg);

    // rdx:rax <- rax * r/m
    printf("xor rdx, rdx\n");
    printf("mov rax, %s\n", reg_names_64[node->reg]);
    printf("imul %s\n", reg_names_64[node->right->reg]);
    printf("mov %s, rax\n", reg_names_64[node->reg]);
    return;
  }
  if (node->op->type == kTokenSlash) {
    Generate(node->left);
    Generate(node->right);
    node->reg = node->left->reg;
    FreeReg(node->right->reg);

    // rax <- rdx:rax / r/m
    printf("xor rdx, rdx\n");
    printf("mov rax, %s\n", reg_names_64[node->reg]);
    printf("idiv %s\n", reg_names_64[node->right->reg]);
    printf("mov %s, rax\n", reg_names_64[node->reg]);
    return;
  }
  if (node->op->type == kTokenPercent) {
    Generate(node->left);
    Generate(node->right);
    node->reg = node->left->reg;
    FreeReg(node->right->reg);

    // rdx <- rdx:rax / r/m
    printf("xor rdx, rdx\n");
    printf("mov rax, %s\n", reg_names_64[node->reg]);
    printf("idiv %s\n", reg_names_64[node->right->reg]);
    printf("mov %s, rdx\n", reg_names_64[node->reg]);
    return;
  }
  if (node->op->type == kTokenShiftLeft) {
    Generate(node->left);
    Generate(node->right);
    node->reg = node->left->reg;
    FreeReg(node->right->reg);

    // r/m <<= CL
    printf("mov rcx, %s\n", reg_names_64[node->right->reg]);
    printf("sal %s, cl\n", reg_names_64[node->reg]);
    return;
  }
  if (node->op->type == kTokenShiftRight) {
    Generate(node->left);
    Generate(node->right);
    node->reg = node->left->reg;
    FreeReg(node->right->reg);

    // r/m >>= CL
    printf("mov rcx, %s\n", reg_names_64[node->right->reg]);
    printf("sar %s, cl\n", reg_names_64[node->reg]);
    return;
  }
  ErrorWithToken(node->op, "Generate: Not implemented");
}

int main(int argc, char *argv[]) {
  struct CompilerArgs args;
  ParseCompilerArgs(&args, argc, argv);

  InitTokenTypeNames();

  fprintf(stderr, "input:\n%s\n", args.input);
  Tokenize(args.input);
  PrintTokens();

  struct ASTNode *ast = Parse();

  printf(".intel_syntax noprefix\n");
  printf(".text\n");
  printf(".global %smain\n", symbol_prefix);
  printf("%smain:\n", symbol_prefix);
  Generate(ast);
  printf("mov rax, %s\n", reg_names_64[ast->reg]);
  printf("ret\n");

  PrintASTNode(ast);
  fputc('\n', stderr);
  return 0;
}
