#Yarn
A small embeddable VM with a custom instruction set and statically allocated
heap.

##Overview
  * Simple instruction set.
  * Sequentially (non-pipelined) execution.
  * Single space memory, stack and heap occupy the same memory space.
  * 16 registers total, 11 multi-purpose.
  * ~25 instructions total.
  * Name inspired by cats.

##Usage:
Compile yarn:
```
./build.sh
```

Assemble your code:
```
./tools/assemble.py code.asm code.o
```

Run it:
```
./bin/yarn code.o
```

If you want to enable some debug features to help you debug your code, compile
yarn with -DYARN_DEBUG. You can pass this argument directly to ./build.sh:
```
./build.sh -DYARN_DEBUG
```

##Embedding and Extending
Embedding is designed to be simple. Here is a simple example of embedding it:
```c
Y = yarn_init(256*sizeof(yarn_int));
yarn_loadCode(Y,buffer,bufsize);
yarn_execute(Y, -1);
yarn_destroy(Y);
```

`yarn_init` takes one argument, which is the number of bytes to allocate.
Typically you will want to multiply this by the size of the basic int type that
yarn uses. `yarn_loadCode` will copy the object code into memory so it can be
executed. `yarn_execute` executes the specified number of instructions, or the
whole program if -1 is specified.

System calls are the main way of extending Yarn. After creating the yarn_state,
you can register system calls to be used with it with the `yarn_registerSysCall`
function. Here is an example of a system call.
```c
static void vyarn_getheight(yarn_state *Y) {
  yarn_setRegister(Y, YARN_REG_RETURN, &screenHeight);
}


Y = yarn_init(256*sizeof(yarn_int));
yarn_registerSysCall(Y, 0xA0, vyarn_getheight);
```
Note you explicitly set the ID for the system call. If there is a preexisting
system call with the same ID, it will overwrite that system call and replace it.

##Memory Layout
All of the program memory is in one chunk. While the amount of possible memory
is set by the environment, if you had 0x400 bytes of memory allocated It could
be visualized like this:

```
Offset  |              Use              |
0x0     |         Program memory        |
0x4     |              ...              |
...           
0x3B8   |         Base of stack         |
0x3BC   |      Register 0xF (%null)     |
0x3C0   |      Register 0xE (%s5)       |
...     |              ...              |
0x3F4   |      Register 0x1 (%bse)      |
0x3F8   |      Register 0x0 (%stk)      |
0x3FC   |    Program status and flags   |
0x400   |         Not allocated         |
```

##Registers
Here is a list of registers
  * *%ins*       -  Instruction pointer
  * *%stk*       -  Stack pointer
  * *%bse*       -  Base pointer
  * *%ret*       -  Return register
  * *%c1* - *%c6*  -  Callee save registers
  * *%s1* - *%s5*  -  Caller save registers (also known as scratch registers)
  * *%null*      -  Null register, used to indicate no register used


##Instructions
Each instruction type has a specific format it uses for encoding. Given here is
the encoding format with the byte offset given. The last byte of the next
instruction is given as context. If two values are 4 bits each a, they are
separated by a `:` and are one byte combined.

### Control instructions
```
| 0x0         | 0x1
| icode:ifun  |
```

These one byte instructions control the program as a whole.
  * *halt*  ( stops the program execution )
  * *pause* ( pauses the execution, however depending on the environment it may
    not get restarted)
  * *nop*   ( does nothing for one instruction )

### Arithmetic instructions
```
| 0x0         | 0x1    |  0x2        | 0x6
| icode:ifun  | rA:rB  | d           |
```

If rA is null then d will be used instead of the value in rA.

Here are the available instructions:
  * *add*
  * *sub*
  * *mul*
  * *div*
  * *divs* (signed)
  * *lsh* (left shift)
  * *rsh* (right shift)
  * *rshs* (right shift, signed)
  * *and*
  * *or*
  * *not*

### Move instructions
```
| 0x0         | 0x1    | 0x2         | 0x6
| icode:ifun  | rA:rB  | d           |
```

If rA is null then d will be used instead of the value in rA.
Although the assembler can figure out which move type is used and only have one
move instruction, the VM cannot implicitly figure it out.
  * *irmov* ( immediate to register )
  * *mrmov* ( memory to register )
  * *rrmov* ( register to register )
  * *rmmov* ( register to memory )

### Stack instructions
```
| 0x0         | 0x1    | 0x2
| icode:ifun  | rA     |
```

These instructions help manipulate the stack pointer.
  * *push*
  * *pop*

### Branch instructions
```
| 0x0         | 0x1          | 0x5
| icode:ifun  | d            |
```

These instructions will jump to a new instruction location.
  * *call*
  * *ret*
  * *jmp*
  * *jif* (jump if, conditional jump will only jump if conditional flag is true)
  * *syscall* (acts the same as call, d will specify which call to make)


### Conditional instructions
```
| 0x0         | 0x1    | 0x2
| icode:ifun  | rA:rB  |
```

These instructions will compare the two registers and set the conditional flag
based on the result. Signed comparisons will interpret the numbers as 2's
complement signed integers.
  * *lt*  ( < )
  * *lts*  ( < signed comparison)
  * *lte* ( <= )
  * *ltes* ( <= signed comparison)
  * *eq*  ( == )
  * *neq* ( != )

##Assembler Syntax Primer
The assembler as it stands is a very crude tool that gets the job done. This
syntax outlined here is subject to change when I get around to improving the
assembler. This is a primer for the syntax the assembler uses. You can use `;`
for a comment.

The basic format is `instruction arg1,arg2` where instruction is a mnemonic
given above, and arg1,arg2 are expressions that usually comprise of registers.

There are several basic types of symbols you can use:

**Registers** are prefixed with % and one of 16 types (but you typically only use
15), and generally have specific purposes associated with them, however this is
just by convention. Example: `%ret`

**Literals** are represented in base 10 or base 16 and are prefixed with `$` or
`0x` respectively to indicate as such. If trying to indicate a negative number,
the negative sign goes in front of the `0x` and after the `$`. Examples: `$255`
`0xFF` `$-255` `-0xFF`

**Locations** are represented by a symbol in this format: `:Name` and get replaced
with the location specified elsewhere in the assembly. They get specified in the
code by `Name:` and get set to whatever address the next instruction is at.
Example:
```x86
:Callee
  ret

:Caller
  call :Callee
```

**Memory addresses** are given by this format: `*(%reg+$offset)` where `%reg` is
the specified register, and `$offset` is a literal specifying the offset in
memory. Either the register or offset can be omitted. Examples: `*(%bse)`
`*(%bse+$8)`.


##System Calls
System calls are used just the same as "call" instructions, except instead of
calling a memory address it will give an ID to specify which function to call.
An example of getting the available memory and storing it in memory address 0x0
is:
```x86
syscall 0x02  ; stores available memory in %ret
mov %ret, *(0x0) ; moves memory from %ret into 0x0
```

|  ID   | C equivalent declaration |                Description               |
| ----: | ------------------------ | ---------------------------------------- |
|  0x00 | uint time()              | Returns the current time of the computer |
|  0x01 | uint cycles()            | Returns the current amount of cycles executed by the VM |
|  0x02 | uint availablememory()   | Returns the number of bytes available to the VM |

##Roadmap
  * Create a proper assembler (with proper errors)  
  * Allow for some sort of linking/combining asm files
  * Debugging tools
