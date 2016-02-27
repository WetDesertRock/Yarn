; The code will start executing at the first instruction. Technically you do not
;   need to label the "Init", but is a nice convention to think of it as a
;   procedure.
Init:
  mov $3, %c1

  push %c1
  call :Multiply_by_five
  add $4, %stk ; remove c1 from stack

  halt ; Needed for graceful shutdown


Multiply_by_five:
  push %bse       ; Stack set up
  mov %stk, %bse

  mov *(%bse+$8), %ret

  mul $5,%ret

  pop %bse   ; Stack tear down
  ret
