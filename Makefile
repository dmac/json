.DEFAULT_GOAL := json_test

.PHONY: run test clean

json_test: json_test.c json.h
	@$(CC) -o $@ $< -std=c99 -g -lm -Wall -Werror -pedantic

test: json_test
	@./$<
	@rm -fr $< $<.dSYM

run: test

clean:
	rm -fr json_test json_test.dSYM
