all:
	gcc -o shell shell.c
	gcc -o scheduler scheduler.c
	gcc user_program.c
shell:
	./shell
