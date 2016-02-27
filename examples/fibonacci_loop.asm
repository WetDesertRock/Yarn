Init:
  mov $20, %c1

  push %c1
  call :Fibonacci
  pop %c1

  halt

; Fibonacci(int i)
;   Finds the i'th number of the fibonacci sequence with a loop.
Fibonacci:
  push %bse
  mov %stk, %bse

  mov *(%bse+$8), %ret
  mov $1, %s5

  lt %ret, %s5      ; if i < 1 return 0
  jif :Fibonacci_EndZero

  eq %ret, %s5         ; if i == 1 return 1
  jif :Fibonacci_End

  mov $1, %s1   ; f(n-1)
  mov $0, %s2   ; f(n-2)
  mov %ret, %s3 ; counter

Fibonacci_Loop:
  mov %s1, %ret
  add %s2, %ret
  mov %s1, %s2
  mov %ret, %s1

  sub $1, %s3
  lt %s3, %s5
  jif :Fibonacci_End  ; if counter < 1 break
  jmp :Fibonacci_Loop ; else continue

Fibonacci_EndZero:    ; zero out %ret and return
  mov $0, %ret

Fibonacci_End:
  pop %bse
  ret
