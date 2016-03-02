#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "yarn.h"

#ifndef YARN_MAP_COUNT
#define YARN_MAP_COUNT 256 // Has to be a power-of-two
#endif
#define YARN_MAP_MASK  (YARN_MAP_COUNT - 1)

struct yarn_state {
  char *code;               // The code that we will execute
  size_t codesize;          // The code size
  void *memory;             // Memory for the program. Contains registers, flags, everything
  size_t memsize;           // The total size of memory
  size_t instructioncount;  // Total count of instuctions used
  // Sys call hash map data structure:
  struct { unsigned key; yarn_CFunc val; } syscalls[YARN_MAP_COUNT];
};

// Syscalls:
static void yarn_sys_gettime(yarn_state *Y) {
  yarn_int t = time(NULL);
  yarn_setRegister(Y, YARN_REG_RETURN, &t);
}
static void yarn_sys_getinstructioncount(yarn_state *Y) {
  yarn_setRegister(Y, YARN_REG_RETURN, &Y->instructioncount);
}
static void yarn_sys_getvmmemory(yarn_state *Y) {
  yarn_setRegister(Y, YARN_REG_RETURN, &Y->memsize);
}

// yarn_functions
yarn_state *yarn_init(size_t memsize) {
  yarn_uint stackaddr;
  yarn_uint instaddr;
  yarn_state *Y = malloc(sizeof(yarn_state));
  if (Y == NULL) {
    return NULL;
  }
  Y->code = NULL;
  Y->memsize = memsize;
  Y->memory = calloc(memsize,1);
  if (Y->memory == NULL) {
    free(Y);
    return NULL;
  }

  //Init our instruction pointer to 0
  instaddr = 0;
  yarn_setRegister(Y, YARN_REG_INSTRUCTION, &instaddr);

  // Init our stack&base pointer to the top of the memory, stack will grow down.
  //   Leave space for the registers, flags, and other stuff at the top.
  stackaddr =  memsize-(YARN_REG_NUM+2)*sizeof(yarn_uint);
  yarn_setRegister(Y, YARN_REG_STACK, &stackaddr);
  yarn_setRegister(Y, YARN_REG_BASE, &stackaddr);

  struct { yarn_uint id; yarn_CFunc fn; } syscalls[] = {
    { 0x00,  yarn_sys_getvmmemory               },
    { 0x01,  yarn_sys_getinstructioncount       },
    { 0x02,  yarn_sys_gettime                   },
    { 0x00,  NULL                               }
  };
  for (int i = 0; syscalls[i].fn; i++) {
    yarn_registerSysCall(Y, syscalls[i].id, syscalls[i].fn);
  }

  return Y;
}

void yarn_destroy(yarn_state *Y) {
  free(Y->code);
  free(Y->memory);
  free(Y);
}

// Will copy given code to an internal buffer.
int yarn_loadCode(yarn_state *Y, char *code, size_t codesize) {
  free(Y->code);

  Y->code = malloc(codesize);
  if (Y->code == NULL) {
    return -1;
  }
  memcpy(Y->code, code, codesize);
  Y->codesize = codesize;
  return 0;
}

// Returns the pointer to its memory.
void *yarn_getMemoryPtr(yarn_state *Y) {
  return Y->memory;
}
size_t yarn_getMemorySize(yarn_state *Y) {
  return Y->memsize;
}
size_t yarn_getInstructionCount(yarn_state *Y) {
  return Y->instructioncount;
}

const char *yarn_registerToString(unsigned char reg) {
  const char *result;
  switch(reg) {
    case YARN_REG_INSTRUCTION: result = "%ins"; break;
    case YARN_REG_STACK: result = "%stk"; break;
    case YARN_REG_BASE: result = "%bse"; break;
    case YARN_REG_RETURN: result = "%ret"; break;
    case YARN_REG_C1: result = "%C1"; break;
    case YARN_REG_C2: result = "%C2"; break;
    case YARN_REG_C3: result = "%C3"; break;
    case YARN_REG_C4: result = "%C4"; break;
    case YARN_REG_C5: result = "%C5"; break;
    case YARN_REG_C6: result = "%C6"; break;
    case YARN_REG_S1: result = "%S1"; break;
    case YARN_REG_S2: result = "%S2"; break;
    case YARN_REG_S3: result = "%S3"; break;
    case YARN_REG_S4: result = "%S4"; break;
    case YARN_REG_S5: result = "%S5"; break;
    case YARN_REG_NULL: result = "%null"; break;
    default:
      result = "invalid";
  }
  return result;
}

const char *yarn_statusToString(int status) {
  const char *result;
  switch(status) {
    case YARN_STATUS_OK: result = "ok"; break;
    case YARN_STATUS_HALT: result = "halt"; break;
    case YARN_STATUS_PAUSE: result = "paused"; break;
    case YARN_STATUS_INVALIDMEMORY: result = "invalid memory access error"; break;
    case YARN_STATUS_INVALIDINSTRUCTION: result = "invalid instruction error"; break;
    case YARN_STATUS_DIVBYZERO: result = "divide by zero  error"; break;
    default:
      result = "invalid";
  }
  return result;
}

// Shortcuts for register manipulation
#define registerLocation(r) Y->memsize-(yarn_uint)(r+2)*sizeof(yarn_uint)
void yarn_getRegister(yarn_state *Y, unsigned char reg, void *val) {
  yarn_getMemory(Y, registerLocation(reg), val, sizeof(yarn_uint));
}
void yarn_setRegister(yarn_state *Y, unsigned char reg, void *val) {
  yarn_setMemory(Y, registerLocation(reg), val, sizeof(yarn_uint));
}
void yarn_incRegister(yarn_state *Y, unsigned char reg, yarn_int val) {
  yarn_int rval;
  yarn_getRegister(Y, reg, &rval);
  rval += val;
  yarn_setRegister(Y, reg, &rval);
}
#undef registerLocation

// Memory manipulations.
void yarn_getMemory(yarn_state *Y, yarn_uint pos, void *val, size_t bsize) {
  if ((pos+bsize) > Y->memsize) { // Check for out-of-bounds
    yarn_setStatus(Y, YARN_STATUS_INVALIDMEMORY);
    return;
  }
  memcpy(val, ((char*)Y->memory)+pos, bsize);
}
void yarn_setMemory(yarn_state *Y, yarn_uint pos, void *val, size_t bsize) {
  if ((pos+bsize) > Y->memsize) { // Check for out-of-bounds
    yarn_setStatus(Y, YARN_STATUS_INVALIDMEMORY);
    return;
  }
  memcpy(((char*)Y->memory)+pos, val,  bsize);
}

// Pushs the stack
void yarn_push(yarn_state *Y, yarn_int val) {
  yarn_uint stk;
  yarn_incRegister(Y, YARN_REG_STACK, -(int)sizeof(yarn_int));
  yarn_getRegister(Y, YARN_REG_STACK, &stk);
  yarn_setMemory(Y, stk, &val, sizeof(val));
}
// Pops the stack
yarn_int yarn_pop(yarn_state *Y) {
  yarn_uint stk;
  yarn_int val;
  yarn_getRegister(Y, YARN_REG_STACK, &stk);
  yarn_getMemory(Y, stk, &val, sizeof(val));
  yarn_incRegister(Y, YARN_REG_STACK, (int)sizeof(yarn_int));
  return val;
}

// Gets the status of the execution. Status codes are given by YARN_STATUS_
int yarn_getStatus(yarn_state *Y) {
  unsigned char val;
  yarn_getMemory(Y, Y->memsize-sizeof(yarn_int), &val, sizeof(val));
  return (int)val;
}
void yarn_setStatus(yarn_state *Y, unsigned char val) {
  yarn_setMemory(Y, Y->memsize-sizeof(yarn_int), &val, sizeof(val));
}

// Gets the specified flag. Currently only used for the conditional flag.
int yarn_getFlag(yarn_state *Y, int flag) {
  unsigned char val;
  yarn_getMemory(Y, Y->memsize-3, &val, sizeof(val));
  return (val>>flag)&1;
}
void yarn_setFlag(yarn_state *Y, int flag) {
  unsigned char val;
  yarn_getMemory(Y, Y->memsize-3, &val, sizeof(val));
  val |= 1 << flag;
  yarn_setMemory(Y, Y->memsize-3, &val, sizeof(val));
}
void yarn_clearFlag(yarn_state *Y, int flag) {
  unsigned char val;
  yarn_getMemory(Y, Y->memsize-3, &val, sizeof(val));
  val &= ~(1 << flag);
  yarn_setMemory(Y, Y->memsize-3, &val, sizeof(val));
}

/*
  System call interface, includes helper functions to manipulate the hashmap
*/

static inline unsigned int hash_uint(unsigned int n) {
  return n * 2654435761;
}

void yarn_registerSysCall(yarn_state *Y, yarn_uint key, yarn_CFunc fun) {
  unsigned int n = hash_uint(key);
  for (;;) {
    unsigned idx = n & YARN_MAP_MASK;
    if (Y->syscalls[idx].key == key || Y->syscalls[idx].val == NULL) {
      Y->syscalls[idx].key = key;
      Y->syscalls[idx].val = fun;
      break;
    }
    n++;
  }
}

yarn_CFunc yarn_getSysCall(yarn_state *Y, yarn_uint key) {
  unsigned n = hash_uint(key);
  for (;;) {
    unsigned idx = n & YARN_MAP_MASK;
    if (Y->syscalls[idx].val == NULL) {
      return NULL;
    }
    if (Y->syscalls[idx].key == key) {
      return Y->syscalls[idx].val;
    }
    n++;
  }
}

/*
 *  External function to execute the program. icount is the maximum number of
 *    instructions to execute. Use -1 to indicate indefinite execution. Will
 *    execute until program sets status to anything but YARN_STATUS_OK,
 */
#define arithinst_s_setup() \
  yarn_validInstruction(5);\
  rA = (Y->code[ip+1] & 0xF0)>>4; \
  rB = Y->code[ip+1] & 0x0F; \
  d_s = *((yarn_int *) &Y->code[ip+2]); \
  yarn_getRegister(Y, rB, &valB_s); \
  if (rA == YARN_REG_NULL) { \
    valA_s = d_s; \
  } else { \
    yarn_getRegister(Y, rA, &valA_s); \
  } \

#define arithinst_setup() \
  yarn_validInstruction(5);\
  rA = (Y->code[ip+1] & 0xF0)>>4; \
  rB = Y->code[ip+1] & 0x0F; \
  d = *((yarn_uint *) &Y->code[ip+2]); \
  yarn_getRegister(Y, rB, &valB); \
  if (rA == YARN_REG_NULL) { \
    valA = d; \
  } else { \
    yarn_getRegister(Y, rA, &valA); \
  } \

#define moveinst_setup() \
  yarn_validInstruction(5);\
  rA = (Y->code[ip+1] & 0xF0)>>4; \
  rB = Y->code[ip+1] & 0x0F; \
  d = *((yarn_int *) &Y->code[ip+2]); \
  if (rA == YARN_REG_NULL) { \
    valA = 0; \
  } else { \
    yarn_getRegister(Y, rA, &valA); \
  } \

#define stackinst_setup() \
  yarn_validInstruction(1);\
  rA = (Y->code[ip+1] & 0xF0)>>4; \

#define branchinst_setup() \
  yarn_validInstruction(4); \
  d = *((yarn_int *) &Y->code[ip+1]); \

#define conditionalinst_setup() \
  yarn_validInstruction(1);\
  rA = (Y->code[ip+1] & 0xF0)>>4; \
  rB = Y->code[ip+1] & 0x0F; \
  yarn_getRegister(Y, rA, &valA); \
  yarn_getRegister(Y, rB, &valB); \
  yarn_clearFlag(Y, YARN_FLAG_CONDITIONAL); \

#define conditionalinst_s_setup() \
  yarn_validInstruction(1);\
  rA = (Y->code[ip+1] & 0xF0)>>4; \
  rB = Y->code[ip+1] & 0x0F; \
  yarn_getRegister(Y, rA, &valA_s); \
  yarn_getRegister(Y, rB, &valB_s); \
  yarn_clearFlag(Y, YARN_FLAG_CONDITIONAL); \

#ifdef YARN_DEBUG
#define yarn_validInstruction(o) if (ip+o >= Y->codesize) { \
  yarn_setStatus(Y,YARN_STATUS_INVALIDINSTRUCTION); \
  printf("INVALID: %d %%ins: 0x%X\n",__LINE__,ip); \
  break; \
}
#else
#define yarn_validInstruction(o) if (ip+o >= Y->codesize) { \
  yarn_setStatus(Y,YARN_STATUS_INVALIDINSTRUCTION); \
  break; \
}
#endif

int yarn_execute(yarn_state *Y, int icount) {
  yarn_uint ip;
  int instruction;

  while (yarn_getStatus(Y) == YARN_STATUS_OK && (icount > 0 || icount == -1)) {
    yarn_getRegister(Y, YARN_REG_INSTRUCTION, &ip);
    yarn_validInstruction(0);

    unsigned char rA, rB;
    yarn_uint valA, valB, valM, d;
    yarn_int valA_s, valB_s, d_s;

    instruction = Y->code[ip];
    #if YARN_DEBUG
    printf("instruction: 0x%02X icode: 0x%02X\n",instruction,instruction & 0xF0);
    #endif

    // Here we execute the specified function for the icode and increment the
    // instruction register.
    switch(instruction) {
      //   Control
      case YARN_INST_HALT:
        yarn_setStatus(Y,YARN_STATUS_HALT);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 1);
        break;
      case YARN_INST_PAUSE:
        yarn_setStatus(Y,YARN_STATUS_PAUSE);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 1);
        break;
      case YARN_INST_NOP:
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 1);
        break;

      //   Arith:
      case YARN_INST_ADD:
        arithinst_setup();
        valB += valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_SUB:
        arithinst_setup();
        valB -= valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_MUL:
        arithinst_setup();
        valB *= valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_DIV:
        arithinst_setup();
        if (valA == 0) {
          yarn_setStatus(Y, YARN_STATUS_DIVBYZERO);
        } else {
          valB /= valA;
          yarn_setRegister(Y, rB, &valB);
        }
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_DIVS:
        arithinst_s_setup();
        if (valA_s == 0) {
          yarn_setStatus(Y, YARN_STATUS_DIVBYZERO);
        } else {
          valB_s /= valA_s;
          yarn_setRegister(Y, rB, &valB_s);
        }
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_LSH:
        arithinst_setup();
        valB <<= valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_RSH:
        arithinst_setup();
        valB >>= valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_RSHS:
        arithinst_s_setup();
        valB_s >>= valA_s;
        yarn_setRegister(Y, rB, &valB_s);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_AND:
        arithinst_setup();
        valB &= valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_OR:
        arithinst_setup();
        valB |= valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_XOR:
        arithinst_setup();
        valB ^= valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_NOT:
        arithinst_setup();
        valB = ~valA;
        yarn_setRegister(Y, rB, &valB);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;

      //   Move:
      case YARN_INST_IR:
        moveinst_setup();
        valA += d;
        yarn_setRegister(Y, rB, &valA);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_MR:
        moveinst_setup();
        yarn_getMemory(Y,d+valA, &valM, sizeof(valM));
        yarn_setRegister(Y,rB,&valM);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_RR:
        moveinst_setup();
        yarn_setRegister(Y,rB,&valA); // Do we want to use d?
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;
      case YARN_INST_RM:
        moveinst_setup();
        yarn_getRegister(Y, rB, &valB);
        yarn_setMemory(Y, valB+d, &valA, sizeof(valA));
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 6);
        break;

      //   Stack:
      case YARN_INST_PUSH:
        stackinst_setup();
        yarn_getRegister(Y, rA, &valA);
        yarn_push(Y, valA);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;
      case YARN_INST_POP:
        stackinst_setup();
        valA = yarn_pop(Y);
        yarn_setRegister(Y, rA, &valA);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;

      //   Branches:
      case YARN_INST_CALL:
        branchinst_setup();
        yarn_push(Y, ip+5);
        yarn_setRegister(Y, YARN_REG_INSTRUCTION, &d);
        break;
      case YARN_INST_RET:
        branchinst_setup();
        for(yarn_uint i=0; i<d; i++) {
          yarn_pop(Y);
        }
        valA = yarn_pop(Y);
        yarn_setRegister(Y, YARN_REG_INSTRUCTION, &valA);
        break;
      case YARN_INST_JUMP:
        branchinst_setup();
        yarn_setRegister(Y, YARN_REG_INSTRUCTION, &d);
        break;
      case YARN_INST_CONDJUMP:
        branchinst_setup();
        if (yarn_getFlag(Y, YARN_FLAG_CONDITIONAL)) {
          yarn_setRegister(Y, YARN_REG_INSTRUCTION, &d);
        } else {
          yarn_incRegister(Y, YARN_REG_INSTRUCTION, 5);
        }
        break;
      case YARN_INST_SYSCALL:
        branchinst_setup();
        yarn_CFunc fun = yarn_getSysCall(Y, (yarn_uint)d);
        if (fun == NULL) {
          yarn_setStatus(Y, YARN_STATUS_INVALIDINSTRUCTION);
        } else {
          (*fun)(Y);
        }
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 5);
        break;

      //   Conditionals:
      case YARN_INST_LT:
        conditionalinst_setup();
        if (valA < valB) yarn_setFlag(Y, YARN_FLAG_CONDITIONAL);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;
      case YARN_INST_LTS:
        conditionalinst_s_setup();
        if (valA_s < valB_s) yarn_setFlag(Y, YARN_FLAG_CONDITIONAL);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;
      case YARN_INST_LTE:
        conditionalinst_setup();
        if (valA <= valB) yarn_setFlag(Y, YARN_FLAG_CONDITIONAL);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;
      case YARN_INST_LTES:
        conditionalinst_s_setup();
        if (valA_s <= valB_s) yarn_setFlag(Y, YARN_FLAG_CONDITIONAL);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;
      case YARN_INST_EQ:
        conditionalinst_setup();
        if (valA == valB) yarn_setFlag(Y, YARN_FLAG_CONDITIONAL);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;
      case YARN_INST_NEQ:
        conditionalinst_setup();
        if (valA != valB) yarn_setFlag(Y, YARN_FLAG_CONDITIONAL);
        yarn_incRegister(Y, YARN_REG_INSTRUCTION, 2);
        break;
      default:
        yarn_setStatus(Y,YARN_STATUS_INVALIDINSTRUCTION);
        break;
    }

    // Check and make sure we haven't run out of instructions to use.
    Y->instructioncount += 1;
    if (icount != -1) {
      icount -= 1;
    }
  }
  return yarn_getStatus(Y);
}

#undef arithinst_setup
#undef arithinst_s_setup
#undef moveinst_setup
#undef stackinst_setup
#undef branchinst_setup
#undef conditionalinst_setup
#undef conditionalinst_s_setup
#undef yarn_validInstruction

#ifdef YARN_STANDALONE
/*
 * Command line program.
 *   Usage: ./bin/yarn code.o
 *   Flags:
 *     -m<file> - Dumps the memory state to a file. Ex: -mmemdump.mem
 *     -c<icount> - Limits execution to icount instructions. Ex: -c20
 */
inline static void printProgramStatus(yarn_state *Y) {
  printf("Register contents:\n");
  yarn_uint rval;
  for (int r=0; r < 16; r++) {
    yarn_getRegister(Y, r, &rval);
    printf("\tReg: %-5s = 0x%08X   %d\n",yarn_registerToString(r), rval, rval);
  }

  printf("Status: %s\n",yarn_statusToString(yarn_getStatus(Y)));
  printf("Instructions executed: %zu\n",yarn_getInstructionCount(Y));

}
int main(int argc, char **argv) {
  FILE *fp;
  size_t fsize;
  yarn_state *Y;
  char *buffer;
  char *memoryfile = NULL;
  int icount = -1;
  int status = YARN_STATUS_OK;

  if (argc <= 1) {
    printf("Must specify an object file to load.\n");
    return 0;
  }
  for (int i=2; i<argc; i++) {
    if (strncmp("-m", argv[i], strlen("-m")) == 0) {
      memoryfile = argv[i]+2;
    } else if (strncmp("-c", argv[i], strlen("-c")) == 0) {
      icount = atoi(argv[i]+2);
    }
  }

  fp = fopen(argv[1], "r");
  if (!fp) {
    printf("Invalid object file.\n");
    return EXIT_FAILURE;
  }
  fseek(fp, 0L, SEEK_END);
  fsize = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  buffer = malloc(sizeof(char)*fsize);
  if (buffer == NULL) {
    printf("Unable to load object file.\n");
    return EXIT_FAILURE;
  }
  fread(buffer,1,fsize,fp);
  fclose(fp);

  Y = yarn_init(256*sizeof(yarn_int));
  if (Y == NULL) {
    printf("Unable to create Yarn state.\n");
    return EXIT_FAILURE;
  }
  if (yarn_loadCode(Y,buffer,fsize) != 0) {
    printf("Unable to load Yarn object code.\n");
    return EXIT_FAILURE;
  }

  while (status == YARN_STATUS_OK) {
    status = yarn_execute(Y, icount);
    printProgramStatus(Y);
    if (status == YARN_STATUS_PAUSE) {
      printf("Program paused, hit enter to continue.");
      getchar();
      // Reset our status
      yarn_setStatus(Y, YARN_STATUS_OK);
      status = YARN_STATUS_OK;
    }
  }

  if (memoryfile != NULL) {
    fp = fopen(memoryfile, "w");
    if (!fp) {
      printf("Invalid memory dump name.\n");
      return EXIT_FAILURE;
    }
    fwrite(yarn_getMemoryPtr(Y), 1, yarn_getMemorySize(Y), fp);
    fclose(fp);
    printf("Wrote memory dump: %s\n",memoryfile);
  }

  yarn_destroy(Y);
  free(buffer);
  return 0;
}
#endif
