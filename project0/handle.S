	.file	"handle.c"
	.section	.rodata
.LC0:
	.string	"My PID: %d\n"
.LC1:
	.string	"Still here"
	.text
.globl main
	.type	main, @function
main:
	pushl	%ebp
	movl	%esp, %ebp
	andl	$-16, %esp
	subl	$48, %esp
	call	getpid
	movl	$.LC0, %edx
	movl	%eax, 4(%esp)
	movl	%edx, (%esp)
	call	printf
	movl	$sigint_handler, 4(%esp)
	movl	$2, (%esp)
	call	Signal
	movl	$sigusr_handler, 4(%esp)
	movl	$10, (%esp)
	call	Signal
.L4:
	movl	$1, 36(%esp)
	movl	$0, 40(%esp)
	movl	$-1, 44(%esp)
	jmp	.L2
.L3:
	leal	28(%esp), %eax
	movl	%eax, 4(%esp)
	leal	36(%esp), %eax
	movl	%eax, (%esp)
	call	nanosleep
	movl	%eax, 44(%esp)
	movl	28(%esp), %eax
	movl	32(%esp), %edx
	movl	%eax, 36(%esp)
	movl	%edx, 40(%esp)
.L2:
	cmpl	$-1, 44(%esp)
	je	.L3
	movl	$.LC1, (%esp)
	call	puts
	jmp	.L4
	.size	main, .-main
	.section	.rodata
.LC2:
	.string	"Nice Try.\n"
	.text
.globl sigint_handler
	.type	sigint_handler, @function
sigint_handler:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	movl	$1, -16(%ebp)
	movl	$10, 8(%esp)
	movl	$.LC2, 4(%esp)
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	write
	movl	%eax, -12(%ebp)
	cmpl	$10, -12(%ebp)
	je	.L8
	movl	$-999, (%esp)
	call	exit
.L8:
	leave
	ret
	.size	sigint_handler, .-sigint_handler
	.section	.rodata
.LC3:
	.string	"exiting\n"
	.text
.globl sigusr_handler
	.type	sigusr_handler, @function
sigusr_handler:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$40, %esp
	movl	$1, -16(%ebp)
	movl	$8, 8(%esp)
	movl	$.LC3, 4(%esp)
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	write
	movl	%eax, -12(%ebp)
	cmpl	$10, -12(%ebp)
	je	.L10
	movl	$-999, (%esp)
	call	exit
.L10:
	movl	$1, (%esp)
	call	exit
	.size	sigusr_handler, .-sigusr_handler
	.ident	"GCC: (Ubuntu 4.4.3-4ubuntu5.1) 4.4.3"
	.section	.note.GNU-stack,"",@progbits
