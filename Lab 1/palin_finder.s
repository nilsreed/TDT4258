.global _start

.section .text

_start:
	// Here your execution starts
	b palindrom_not_found
	b exit

	
check_input:
	// You could use this symbol to check for your input length
	// you can assume that your input string is at least 2 characters 
	// long and ends with a null byte
	
	
check_palindrome:
	// Here you could check whether input is a palindrome or not
	
	
palindrome_found:
	// Switch on only the 5 rightmost LEDs
	// Write 'Palindrome detected' to UART

	// TURN ON LEDS HERE

	ldr r0, =found
	b print_output
	
	
palindrom_not_found:
	// Switch on only the 5 leftmost LEDs
	// Write 'Not a palindrome' to UART

	// TURN ON LEDS HERE

	ldr r0, =n_found
	b print_output


/**
* Assumes its branched to from palindrome_found or
* palindrom_not_found and sends letters from the
* string whose address has been loaded into r0 
* Assumes there is always write space and 
* thus doesn't check the WSPACE field of the control register
*
* See p17 in Intel FPGA University Program DE1-SoC Computer Manual
*/
print_output:
	ldr r1, =0xFF201000 // Load JTAG UART base address
print_loop:
	ldrb r2, [r0]		// Load next character into r2
	cmp r2, #0			// Check if it's 0, which signifies its end
	beq exit			// Exit program if r2 was 0
	str r2, [r1]		// Write character to DATA field of JTAG data register
	add r0, r0, #1		// Increment memory address to fetch next character in string
	b print_loop


	
exit:
	// Branch here for exit
	b exit
	

.section .data
.align
	// This is the input you are supposed to check for a palindrome
	// You can modify the string during development, however you
	// are not allowed to change the label 'input'!
	input: .asciz "level"
	// input: .asciz "8448"
    // input: .asciz "KayAk"
    // input: .asciz "step on no pets"
    // input: .asciz "Never odd or even"
	
	// Save output strings in data section for convenience
	found: .asciz "Palindrome detected\n"
	n_found: .asciz "Not a palindrome\n"

.end
