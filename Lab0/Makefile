lab0:	lab0.c
	gcc -g -o lab0 lab0.c

clean:
	rm lab0

check:	lab0
	echo hello world > hello
	./lab0 < hello
	./lab0 --input=hello
	./lab0 --output=world < hello
	echo hello, world!!!! > hello
	./lab0 --input=hello --output=world
	./lab0 --segfault --input=hello || ([ $$? -eq 139 ] && echo "Return code for segfault OK")
	./lab0 --catch --input=hello
	./lab0 --segfault --catch --input=hello || ([ $$? -eq 3 ] && echo "Return code for catch OK")
	./lab0 --segfault --catch --input=hello --output=world || ([ $$? -eq 3 ] && echo "Return code for catch OK")
	chmod 000 world
	./lab0 --input=hello --output=world || ([ $$? -eq 2 ] && echo "Return code for output OK")
	./lab0 --input=world || ([ $$? -eq 1 ] && echo "Return code for input OK")
	chmod 744 world
	rm hello world

dist:
	tar -czf lab0-104485521.tar.gz backtrace.png nullptr.png Makefile lab0.c README
