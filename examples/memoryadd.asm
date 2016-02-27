Init:
  mov 0x0, %c1 ; Start address
  mov 0xAF, %c2 ; Size

  push %c2
  push %c1
  call :FillMemory
  call :SumMemory
  add $8, %stk

  halt ; Needed for graceful shutdown


; FillMemory(int *start, int size)
;   Fills the memory starting at *start with `size` deincrementing numbers
FillMemory:
  push %bse       ; Stack set up
  mov %stk, %bse

  mov $0, %s5
  mov *(%bse+$8), %s1  ; *start
  mov *(%bse+$12), %s2   ; size

  lte %s2, %s5
  jif :FillMemory_End
FillMemory_Loop:
  mov %s2, *(%s1) ; *start = counter

  add $4, %s1
  sub $1, %s2
  lte %s2, %s5
  jif :FillMemory_End   ; if size <= 0 then break
  jmp :FillMemory_Loop  ; else loop

FillMemory_End:
  pop %bse   ; Stack tear down
  ret
; End FillMemory

; SumMemory(int *start, int size)
SumMemory:
  push %bse       ; Stack set up
  mov %stk, %bse

  mov $0, %s5
  mov *(%bse+$8), %s1  ; *start
  mov *(%bse+$12), %s2   ; size
  mov $0, %ret          ; sum

  lte %s2, %s5
  jif :Sum_End

Sum_Loop:
  mov *(%s1), %s3
  add %s3, %ret

  add $4, %s1
  sub $1, %s2
  lte %s2, %s5
  jif :Sum_End   ; if size <= 0 then break
  jmp :Sum_Loop  ; else loop

Sum_End:
  pop %bse   ; Stack tear down
  ret
; End SumMemory
