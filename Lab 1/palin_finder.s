.global _start

.section .text

_start:
	// Here your execution starts
	b check_input


/**
* Loops through input, corrects lower case letters to upper case
* and counts the number of letters.
*/
check_input:
	// You could use this symbol to check for your input length
	// you can assume that your input string is at least 2 characters 
	// long and ends with a null byte
	ldr r0, =0			// Counter for string length
	ldr r1, =input		// Load address of input
check_loop:
	ldrb r2, [r1]		// Load character from string
	cmp r2, #0			// Check if end of string
	beq check_done
	add r0, r0, #1		// Increment counter for string length
	cmp r2, #90			// Check if character is lower case letter, see ASCII table
	blt correct_case	// Branch if character is not lower case letter
	sub r2, r2, #32		// Else, subtract 32 (to make it upper case), see ASCII table
	strb r2, [r1]		// Store case-corrected character for later
correct_case:
	add r1, r1, #1		// Increment memory location
	b check_loop
check_done:
	b check_palindrome
	
/**
*
* Assumes the number of characters is stored in r0
*/
check_palindrome:
	ldr r1, =input			// Load address of input string
	add r2, r1, r0			// calculate address of last character in string
	sub r1, r1, #1			// Decrement the index in the memory for simplifying logic
	ldr r3, =0x20			// ASCII code for space, for comparison
palindrome_loop:
	cmp r1, r2				// Check if the addresses have crossed each other (meaning we've traversed the string in its entirety)
	bgt palindrome_found
space_loop_left:			// Loop to make sure we skip whitespace when traversing from the left
	add r1, r1, #1			// Increment the index in the string
	ldrb r4, [r1]			// Load new character to compare
	cmp r4, r3				// Compare newly fetched character to space
	bne space_loop_right	// Go on if it isn't
	b space_loop_left		
space_loop_right:			// Loop to make sure we skip whitespace when traversing from the right
	sub r2, r2, #1			// Decrement the index in the string
	ldrb r5, [r2]			// Load new character to compare
	cmp r5, r3				// Compare newly fetched character to space
	bne no_spaces			// Go on if it isn't
	b space_loop_right		
no_spaces:
	cmp r4, r5				// Compare characters
	bne palindrom_not_found
	b palindrome_loop
	// TODO check strings, break to not found if not equal. Remember to skip white space..
	
	// Here you could check whether input is a palindrome or not
	
	
palindrome_found:
	ldr r0, =0xFF200000		// Load address of red LED data register
	ldr r1, =0x1F			// 1s in bits 0-4
	str r1, [r0]			// Write to data register to turn leds on

	ldr r0, =found			// Load correct string address into r0
	b print_output
	
	
palindrom_not_found:
	ldr r0, =0xFF200000		// Load address of red LED data register
	ldr r1, =0x3E0			// 1s in bits 5-9
	str r1, [r0]			// Write to data register to turn leds on

	ldr r0, =n_found		// Load correct string address into r0
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
	// input: .asciz "level"
	// input: .asciz "8448"
    // input: .asciz "KayAk"
    // input: .asciz "step on no pets"
    // input: .asciz "Never odd or even"
	input: .asciz "Clearly not palindromic"
	
	// Save output strings in data section for convenience
	found: .asciz "Palindrome detected\n"
	n_found: .asciz "Not a palindrome\n"

.end
