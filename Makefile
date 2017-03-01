.DEFAULT_GOAL := all

.PHONY: all test run clean

all: json_test jsoncat

test: json_test
	@./$<
	@make clean

run: test

clean:
	@rm -fr jsoncat json_test *.dSYM

json_test: json_test.c json.h
	$(CC) -o $@ $< -std=c99 -O2 -Wall -Werror -pedantic

jsoncat: jsoncat.c json.h
	$(CC) -o $@ $< -std=c99 -O2 -Wall -Werror -pedantic

