// Usage:
//   These .c and .h files only deal with assembled code. To create code to be
//   read, you either need to assemble it by hand (with a hex editor and lots of
//   patience), or you can use the included assembler.
//   Most typical usage will just require setting up the state, loading in the
//   instructions, executing, and destroying after you are finished.
//   This will usually look like:
//     Y = yarn_init(256*sizeof(yarn_int));
//     yarn_loadCode(Y,buffer,bufsize);
//     yarn_execute(Y, -1);
//     yarn_destroy(Y);
//
//   Most of this is self explanitory, the only surprising thing may be the
//   `yarn_execute` call's second argument. This argument specifies how many
//   instructions it should execute. -1 will execute forever. This is useful if
//   you want to embed yarn elsewhere and don't want it to dominate your program.
//
// Extending:
//   Currently there is no API for extending it with "system calls", however this
//   is planned for the future.

#include <stdint.h>
#include <stdlib.h>

#ifndef YARN_H_
#define YARN_H_

#define YARN_VERSION "0.0.1"

typedef struct yarn_state yarn_state;
typedef int32_t yarn_int;
typedef uint32_t yarn_uint;
typedef void (*yarn_CFunc)(yarn_state *Y);

// Create the yarn state, returns NULL on failure
yarn_state *yarn_init(size_t memsize);
// Destroys the yarn state.
void yarn_destroy(yarn_state *Y);
// Loads the object code, returns 0 on success, -1 on failure.
int yarn_loadCode(yarn_state *Y, char *code, size_t codesize);
// Executes icount instructions (-1 for the whole program). Returns the status.
int yarn_execute(yarn_state *Y, int icount);

void *yarn_getMemoryPtr(yarn_state *Y);
size_t yarn_getMemorySize(yarn_state *Y);
size_t yarn_getInstructionCount(yarn_state *Y);

const char *yarn_registerToString(unsigned char reg);
const char *yarn_statusToString(int status);

void yarn_getRegister(yarn_state *Y, unsigned char reg, void *val) ;
void yarn_setRegister(yarn_state *Y, unsigned char reg, void *val);
void yarn_incRegister(yarn_state *Y, unsigned char reg, yarn_int val);

void yarn_getMemory(yarn_state *Y, yarn_uint pos, void *val, size_t bsize);
void yarn_setMemory(yarn_state *Y, yarn_uint pos, void *val, size_t bsize);

void yarn_push(yarn_state *Y, yarn_int val);
yarn_int yarn_pop(yarn_state *Y);

int yarn_getStatus(yarn_state *Y);
void yarn_setStatus(yarn_state *Y, unsigned char val);

int yarn_getFlag(yarn_state *Y, int flag);
void yarn_setFlag(yarn_state *Y, int flag);
void yarn_clearFlag(yarn_state *Y, int flag);

void yarn_registerSysCall(yarn_state *Y, yarn_uint key, yarn_CFunc fun);
yarn_CFunc yarn_getSysCall(yarn_state *Y, yarn_uint key);

enum {
  YARN_STATUS_OK,
  YARN_STATUS_PAUSE,
  YARN_STATUS_HALT,
  YARN_STATUS_INVALIDMEMORY,
  YARN_STATUS_INVALIDINSTRUCTION,
  YARN_STATUS_DIVBYZERO,
  YARN_STATUS_NUM,
};
enum {
  YARN_FLAG_CONDITIONAL, // 0x0 // Stores result of last COND statement.
  YARN_FLAG_NUM,
};

enum {
  YARN_REG_INSTRUCTION,   //0x0 // Special Registers:
  YARN_REG_STACK,         //0x1
  YARN_REG_BASE,          //0x2
  YARN_REG_RETURN,        //0x3
  YARN_REG_C1,            //0x4 // Callee Save Registers:
  YARN_REG_C2,            //0x5
  YARN_REG_C3,            //0x6
  YARN_REG_C4,            //0x7
  YARN_REG_C5,            //0x8
  YARN_REG_C6,            //0x9
  YARN_REG_S1,            //0xA // Caller (sub or scratch) Save Registers:
  YARN_REG_S2,            //0xB
  YARN_REG_S3,            //0xC
  YARN_REG_S4,            //0xD
  YARN_REG_S5,            //0xE
  YARN_REG_NULL,          //0xF // Used to indicate 'no register' for arith and move operations.
  YARN_REG_NUM,
};

enum {
  YARN_ICODE_CONTROL = 0x00,       //0x00
  YARN_ICODE_ARITH = 0x10,         //0x10
  YARN_ICODE_MOVE = 0x20,          //0x20
  YARN_ICODE_STACK = 0x30,         //0x30
  YARN_ICODE_BRANCH = 0x40,        //0x40
  YARN_ICODE_CONDITIONAL = 0x50,   //0x50
  YARN_ICODE_NUM = 0x60,           //0x60
};

enum {
  YARN_INST_HALT = YARN_ICODE_CONTROL,  //0x00
  YARN_INST_PAUSE,                      //0x01
  YARN_INST_NOP,                        //0x02

  /* ARITH: */
  YARN_INST_ADD = YARN_ICODE_ARITH,   //0x10
  YARN_INST_SUB,                      //0x11
  YARN_INST_MUL,                      //0x12
  YARN_INST_DIV,                      //0x13
  YARN_INST_DIVS,                     //0x14 signed
  YARN_INST_LSH,                      //0x15
  YARN_INST_RSH,                      //0x16
  YARN_INST_RSHS,                     //0x17 signed
  YARN_INST_AND,                      //0x18
  YARN_INST_OR,                       //0x19
  YARN_INST_XOR,                      //0x1A
  YARN_INST_NOT,                      //0x1B

  /* MOVE */
  YARN_INST_IR = YARN_ICODE_MOVE,    //0x20
  YARN_INST_MR,                      //0x21
  YARN_INST_RR,                      //0x22
  YARN_INST_RM,                      //0x23

  /* STACK */
  YARN_INST_PUSH = YARN_ICODE_STACK,  //0x30
  YARN_INST_POP,                      //0x31

  /* BRANCH */
  YARN_INST_CALL = YARN_ICODE_BRANCH,  //0x40
  YARN_INST_RET,                       //0x41
  YARN_INST_JUMP,                      //0x42
  YARN_INST_CONDJUMP,                  //0x43
  YARN_INST_SYSCALL,                   //0x44

  /* CONDITIONAL */
  YARN_INST_LT = YARN_ICODE_CONDITIONAL,    //0x50
  YARN_INST_LTS,                            //0x51 signed
  YARN_INST_LTE,                            //0x52
  YARN_INST_LTES,                           //0x53 signed
  YARN_INST_EQ,                             //0x54
  YARN_INST_NEQ,                            //0x55

  YARN_INST_NUM,
};

#endif
