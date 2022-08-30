.global _start

loop:
	// This example iteratively multiplies r2*r1
	cmp r0, #0     // Compare r0 to 0
	bne .+8        // If not equal branch OVER next instruction
	bx lr          // Branch back to who called loop
	add r2, r2, r1 // Add r1 to r2 and put it on r2
	sub r0, r0, #1 // Substract 1 from r0 and put it on r0
	b loop         // branch back to beginning of loop

_start:
	// Here your execution starts
	mov r0, #10 // Decimal 10 o register r0
	mov r1, #2  // Decimal 2 to register r1
	mov r2, #0  // Decimal 0 to register r2
	bl loop     // Branch and link to loop
	b _exit

_exit:
	// Branch to itelf
	b .
	
.data
.align
	// This section is evaluated before execution to put things into
	// memory that are required for the execution of your application
.end
