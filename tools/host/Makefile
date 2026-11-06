CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer

test: test_fire
	./test_fire

test_fire: test_fire.c ../../src/c/fire.c ../../src/c/fire.h
	$(CC) $(CFLAGS) -o $@ test_fire.c ../../src/c/fire.c

clean:
	rm -f test_fire

.PHONY: test clean
