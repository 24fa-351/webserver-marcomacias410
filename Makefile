echo: echo.c http_message.c
	gcc -o echo echo.c http_message.c

clean:
	rm -f echo
