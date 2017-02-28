.DEFAULT_GOAL := json_test

.PHONY: run test clean

json_test: json_test.c json.h
	@$(CC) -o $@ $< -std=c99 -lm -Wall -Werror -pedantic

test: json_test
	@./$<
	@rm -f $<

run: test

clean:
	rm -f json_test
