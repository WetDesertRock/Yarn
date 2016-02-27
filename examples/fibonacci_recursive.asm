Init:
  mov $20, %c1

  push %c1
  call :Fibonacci
  pop %c1

  halt

; Fibonacci(int i)
;   Recursivly finds the i'th number (starts counting at 0) of the fibonacci
;   sequence.
Fibonacci:
  push %bse
  mov %stk, %bse
  push %c1
  push %c2

  mov *(%bse+$8), %ret
  mov $1, %s1

  lt %ret, %s1      ; if i < 1 return 0
  jif :Fibonacci_EndZero

  eq %ret, %s1         ; if i == 1 return 1
  jif :Fibonacci_End

  mov %ret, %c1 ; n-1
  mov %ret, %c2 ; n-2
  sub $1, %c1
  sub $2, %c2

  ; call Fibonacci(n-1)
  push %c1
  call :Fibonacci
  add $4, %stk

  mov %ret, %c1

  ; call Fibonacci(n-2)
  push %c2
  call :Fibonacci
  add $4, %stk

  add %c1, %ret ; n-1 + n-2
  jmp :Fibonacci_End

Fibonacci_EndZero:    ; zero out %ret and return
  mov $0, %ret

Fibonacci_End:
  pop %c2
  pop %c1
  pop %bse
  ret
