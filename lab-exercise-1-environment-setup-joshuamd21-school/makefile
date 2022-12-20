all: buggy

buggy: buggy.cpp
	g++ -std=c++17 -pedantic -Wall -g -Wextra  -fsanitize=address,undefined -o buggy buggy.cpp

clean:
	rm buggy

run: buggy
	./buggy

val: buggy
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./buggy