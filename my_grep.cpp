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

// Virtual class
class RegexOperator
{
public:
	// Convert the operator to a readable string for debbuging purposes
	virtual string toString() = 0;

	// Add a id to each letter to make it unique
	virtual void linearize(int ids_cache[CHAR_MAX]) = 0;
};

// Operator who will only match a specific character
class RegexBasicChar : public RegexOperator
{
	// The char to match
	char character;
	// Used by the glushkov algo
	int glushkov_id;

public:
	RegexBasicChar(char c)
	{
		character = c;
	}

	virtual string toString()
	{
		string letter;
		int_to_str(character, letter);
		return "Basic(" + letter + ")";
	}
	
	void linearize(int ids_cache[CHAR_MAX])
	{
		glushkov_id = ids_cache[character]++;
	}
};

// Operator who will only match any character
class RegexAnyChar : public RegexOperator
{
public:
	virtual string toString()
	{
		return "Any()";
	}

	void linearize(int ids_cache[CHAR_MAX])
	{
	}
};

// Operator who will only match any number of character
class RegexKleenStar : public RegexOperator
{
public:
	virtual string toString()
	{
		return "Star()";
	}
	
	void linearize(int ids_cache[CHAR_MAX])
	{
	}
};

// Operator who will match one of the Regex in it's options
class RegexOr : public RegexOperator
{
public:
	list<RegexOperator*> options;

	~RegexOr()
	{
		for (RegexOperator* option : options) {
			delete option;
		}
	}

	virtual string toString()
	{
		string str("OR(");

		for (RegexOperator* option : options) {
			str += option->toString() + " | ";
		}

		return str + ")";
	}
	
	void linearize(int ids_cache[CHAR_MAX])
	{
		for (RegexOperator* option : options) {
			option->linearize(ids_cache);
		}
	}
};

// Regex is a list of operators
class Regex : public list<RegexOperator*>
{
public:
	~Regex()
	{
		for (RegexOperator* op : *this) {
			delete op;
		}
	}

	string toString()
	{
		string str;
		for (RegexOperator* op : *this) {
			str += op->toString() + ", ";
		}
		return str;
	}
	
	void linearize(int ids_cache[CHAR_MAX])
	{
		for (RegexOperator* op : *this) {
			op->linearize(ids_cache);
		}
	}
};

class RegexFactory
{
public:
	static void parse(string &input, Regex &output)
	{

	}

	static void linearize(Regex &regex)
	{
		int ids_cache[CHAR_MAX] = {0};

		regex.linearize(ids_cache);
	}

	static void glushkov(Regex &regex)
	{
		RegexFactory::linearize(regex);
	}

	// Regex factory
	static Regex from_cstr(const char* str)
	{
		string input(str);
		Regex regex;

		// Parse the string regex into a node tree
		RegexFactory::parse(input, regex);
		// Convert the node tree into a compiled automaton
		RegexFactory::glushkov(regex);

		if (regex.empty()) {
			throw RegexError("Empty regex", 0, 0);
		}

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
		Regex regex = RegexFactory::from_cstr(argv[2]);
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
