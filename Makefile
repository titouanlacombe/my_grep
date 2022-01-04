default: my_grep

.PHONY: my_grep test

my_grep:
	g++ my_grep.cpp -o my_grep
	
test: my_grep
	./my_grep input.txt "indexes\t"
	