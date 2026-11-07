CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer

test: test_fire test_daylight
	./test_fire
	./test_daylight

test_fire: test_fire.c ../../src/c/fire.c ../../src/c/fire.h
	$(CC) $(CFLAGS) -o $@ test_fire.c ../../src/c/fire.c

test_daylight: test_daylight.c ../../src/c/daylight.c ../../src/c/daylight.h
	$(CC) $(CFLAGS) -o $@ test_daylight.c ../../src/c/daylight.c

crackle_wav: crackle_wav.c ../../src/c/crackle_synth.c ../../src/c/crackle_synth.h
	$(CC) $(CFLAGS) -o $@ crackle_wav.c ../../src/c/crackle_synth.c

crackle.wav: crackle_wav
	./crackle_wav crackle.wav

clean:
	rm -f test_fire test_daylight crackle_wav crackle.wav

.PHONY: test clean
