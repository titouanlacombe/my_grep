#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <string>
#include <list>
#include <vector>
#include <chrono>

#define CHAR_MAX 256

using namespace std;
using namespace std::chrono;

// Print special characters correctly
void int_to_str(int result, string &output)
{
	switch (result) {
	case '\n':
		output.append("\\n");
		return;
	case '\r':
		output.append("\\r");
		return;
	case '\t':
		output.append("\\t");
		return;
	case EOF:
		output.append("EOF");
		return;
	default:
		output.push_back(result);
		return;
	}
}

// Basic exception type
class RegexError : std::exception
{
public:
	string reason;
	int line_number;
	int column_number;

	RegexError(string e, int _line_number, int _column_number)
	{
		reason = e;
		line_number = _line_number;
		column_number = _column_number;
	}

	string what()
	{
		return reason + " at (" + to_string(line_number) + ", " + to_string(column_number) + ")";
	}
};

// Store data about a match in the text
class Match
{
	streampos start, end;

public:
	Match(streampos &_start, streampos &_end)
	{
		start = _start;
		end = _end;
	}

	// Prints pretty line number with padding & a separator
	void print_line_nb(ostream& output, int nb)
	{
		string number = to_string(nb);
		output << " " << number;

		int spacing = 4 - number.size();
		for (size_t i = 0; i < spacing; i++) {
			output << " ";
		}

		output << "| ";
	}

	// Diplay the match with line number & it's context
	void print(ostream& output)
	{
		// Match context start
		// print_line_nb(output, start_line_nb);
		// output << context_start << "\e[44m";

		// int line_nb = start_line_nb;
		// while (stream.peek() != EOF) {
		// 	char c = stream.get();

		// 	if (c == '\n') {
		// 		line_nb++;
		// 		// Remove color for the new line
		// 		output << "\\n\e[0m" << endl;
		// 		print_line_nb(output, line_nb);
		// 		output << "\e[44m";
		// 	}
		// 	else {
		// 		output << c;
		// 	}
		// }

		// Match context end
		// output << "\e[0m" << context_end << endl;
	}
};

// Node of a regex tree
class RegexNode
{
public:
	// Operators are in order of appearance
	list<RegexNode> childrens;

	bool empty()
	{
		return childrens.empty();
	}

	virtual string toString()
	{
		string str = "TreeNode{";
		for (RegexNode &op : childrens) {
			str += op.toString() + ", ";
		}
		return str + "}, ";
	}
	
	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		for (RegexNode& op : childrens) {
			op.linearize(ids_cache);
		}
	}
};

// Operator who will only match a specific character
class CharLeaf : public RegexNode
{
	// The char to match
	char character;
	// ID Used by the glushkov algo
	int glushkov_id;

public:
	CharLeaf(char c)
	{
		character = c;
	}

	virtual string toString()
	{
		string letter;
		int_to_str(character, letter);
		return "Basic(" + letter + ")";
	}
	
	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		glushkov_id = ids_cache[character]++;
		cout << "(letter: " << character << ", id: " << glushkov_id << "), ";
	}
};

// Operator who will only match any character
class AnyLeaf : public RegexNode
{
public:
	virtual string toString()
	{
		return "Any";
	}

	virtual void linearize(int ids_cache[CHAR_MAX])
	{
	}
};

// Operator who will only match any number of character
class KleenStarLeaf : public RegexNode
{
public:
	virtual string toString()
	{
		return "KleenStar";
	}
	
	virtual void linearize(int ids_cache[CHAR_MAX])
	{
	}
};

// Operator who will match one of the regex in it's options
class OrNode : public RegexNode
{
public:
	// Operators are in order of priority
	list<RegexNode> options;

	virtual string toString()
	{
		string str("ORNode[");

		for (RegexNode option : options) {
			str += option.toString() + ", ";
		}

		return str + "]";
	}
	
	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		for (RegexNode option : options) {
			option.linearize(ids_cache);
		}
	}
};

class RegexFactory
{
public:
	static void parse(string &input, RegexNode &output)
	{

	}

	static void linearize(RegexNode &regex)
	{
		int ids_cache[CHAR_MAX] = {0};

		regex.linearize(ids_cache);
	}

	static void glushkov(RegexNode &regex)
	{
		RegexFactory::linearize(regex);
	}

	// RegexNode factory
	static RegexNode from_cstr(const char* str)
	{
		string input(str);
		RegexNode regex;

		// Parse the string regex into a node tree
		RegexFactory::parse(input, regex);

		if (regex.empty()) {
			throw RegexError("Empty regex", 0, 0);
		}
		
		// Convert the node tree into a compiled automaton
		RegexFactory::glushkov(regex);

		// cout << "Raw: '" << str << "'" << endl;
		// cout << "Compiled: " << regex.toString() << endl;

		return regex;
	}
};

int main(int argc, char const* argv[])
{
	ifstream input;
	ostream& output = cout;

	if (argc < 3) {
		cout << "Error: not enough arguments\nUsage: ./my_grep input_file pattern" << endl;
		return -1;
	}

	input.open(argv[1]);
	if (input.fail()) {
		cerr << "Error '" << argv[1] << "': " << strerror(errno) << endl;
		return -2;
	}

	// Compile provided regex & search it in the text
	list<Match> matches;
	auto t0 = high_resolution_clock::now();
	try {
		RegexNode regex = RegexFactory::from_cstr(argv[2]);
		// matches = regex.search_in(input);
	}
	catch (RegexError& e) {
		std::cerr << "Error: " << e.what() << endl;
		return -1;
	}
	auto dt = duration_cast<milliseconds>(high_resolution_clock::now() - t0).count();

	cout << "Execution time: " << dt << " ms" << endl;

	if (matches.size() == 0) {
		cout << "No matches found" << endl;
		return -1;
	}

	// Prints the matches
	cout << "Matches:" << endl;
	for (Match& m : matches) {
		m.print(cout);
		cout << endl;
	}

	input.close();

	return 0;
}
