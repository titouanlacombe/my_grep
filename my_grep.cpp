#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <string>
#include <list>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Help to print special characters
string i_to_string(int result)
{
	switch (result) {
	case '\n':
		return string("\\n");
	case '\r':
		return string("\\r");
	case '\t':
		return string("\\t");
	case EOF:
		return string("EOF");
	default:
		return string(1, result);
	}
}

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

class Match
{
	int start_line_nb;
	string context_start;
	string extracted;
	string context_end;

public:
	Match(int line_nb, string _context_start)
	{
		context_start = _context_start;
		start_line_nb = line_nb;
	}

	void complete(stringstream& _extracted, string _context_end)
	{
		extracted = _extracted.str();
		context_end = _context_end;
	}

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

	void print(ostream& output)
	{
		stringstream stream(extracted);

		// Match context start
		print_line_nb(output, start_line_nb);
		output << context_start << "\e[44m";

		// Match extracted data
		int line_nb = start_line_nb;
		while (stream.peek() != EOF) {
			char c = stream.get();

			if (c == '\n') {
				line_nb++;
				// Remove color for the new line
				output << "\\n\e[0m" << endl;
				print_line_nb(output, line_nb);
				output << "\e[44m";
			}
			else {
				output << c;
			}
		}

		// Match context end
		output << "\e[0m" << context_end << endl;
	}
};

class RegexOperator
{
public:
	virtual bool execute(istream& input, ostream& output, RegexOperator* next) = 0;
	virtual string toString() = 0;
};

class RegexBasicChar : public RegexOperator
{
	char character;

public:
	RegexBasicChar(char c)
	{
		character = c;
	}

	virtual bool execute(istream& input, ostream& output, RegexOperator* next)
	{
		int i = input.get();

		if (i == character) {
			output << (char)i;
			return true;
		}

		return false;
	}

	virtual string toString()
	{
		return "Basic(" + i_to_string(character) + ")";
	}
};

class RegexAnyChar : public RegexOperator
{
public:
	virtual bool execute(istream& input, ostream& output, RegexOperator* next)
	{
		int c = input.get();

		if (c == EOF) {
			return false;
		}

		output << (char)c;
		return true;
	}

	virtual string toString()
	{
		return "Any()";
	}
};

class RegexKleenStar : public RegexOperator
{
public:
	virtual bool execute(istream& input, ostream& output, RegexOperator* next)
	{
		// IF NO NEXT REGEX: MATCH ALL INPUT AND RETURN TRUE
		// RegexOperator* _next;
		// streampos pos;
		// stringstream _output;

		// // While next is not valid add char to match
		// do {
		// 	pos = input.tellg(); // Save pos
		// 	_output.clear();
		// 	_next = next->execute(input, _output);
		// 	input.seekg(pos); // Restore pos

		// 	if (_next == nullptr) {
		// 		// Add one char to match
		// 		output << input.get();
		// 	}
		// }
		// while (!input.eof() && _next == nullptr);

		// // If next successfull add the matched str in output
		// if (_next != nullptr) {
		// 	output << _output.str();
		// }

		return false;
	}

	virtual string toString()
	{
		return "Star()";
	}
};

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

	virtual bool execute(istream& input, ostream& output, RegexOperator* next)
	{
		streampos pos = input.tellg(); // Save pos

		// Test all options
		for (RegexOperator* option : options) {
			stringstream _output;

			input.seekg(pos); // Restore pos

			bool match_success = option->execute(input, _output, next);

			if (next != nullptr) {
				match_success &= next->execute(input, _output, nullptr);
			}

			// If one option successfull take it's matched extracted & return
			if (match_success) {
				output << _output.str();
				return true;
			}
		}

		input.seekg(pos); // Restore pos

		return false;
	}

	virtual string toString()
	{
		string str("OR(");

		for (RegexOperator* option : options) {
			str += option->toString() + " | ";
		}

		return str + ")";
	}
};

class Regex : public list<RegexOperator*>
{
public:
	~Regex()
	{
		for (RegexOperator* op : *this) {
			delete op;
		}
	}

	bool try_match(istream& input, Match& match, streampos& match_end_pos)
	{
		bool operator_success = true;
		stringstream extracted_tmp;
		string context_end;

		// cout << "\nMatch start" << endl;

		// For each operators
		auto it = begin();
		while (operator_success && it != end()) {
			// cout << "Next char: '" << i_to_string(input.peek()) << "'" << endl;
			// cout << "Current op: " << ((*it) == nullptr ? "null" : (*it)->toString()) << endl;
			operator_success = (*it)->execute(input, extracted_tmp, (++it) == end() ? nullptr : *it);
		}

		if (operator_success) {
			// cout << "Match operator_success" << endl;

			// Save the pos before the potential getline
			match_end_pos = input.tellg();

			match.complete(extracted_tmp, context_end);
		}

		// cout << "Match failure" << endl;
		return operator_success;
	}

	void skip_to_pos(istream& input, streampos& new_pos, string& current_line, int& line, int& col)
	{
		while (input.tellg() < new_pos) {
			// Increment pos
			int r = input.get();
			if (r == EOF) {
				return;
			}

			if (r == '\n') {
				current_line.clear();
				line++;
				col = 1;
			}
			else {
				current_line.push_back((char)r);
				col++;
			}
		}
	}

	list<Match> search_in(istream& input)
	{
		list<Match> matches;

		// Position
		string current_line;
		int line = 1;
		int col = 1;

		while (input.peek() != EOF) {
			// Match vars
			streampos saved_pos = input.tellg(), new_pos;
			bool match_success = true;

			Match match(line, current_line);
			match_success = try_match(input, match, new_pos);

			if (match_success) {
				matches.push_back(match);
				// Keep new pos
			}
			else {
				// Indent old pos
				new_pos = saved_pos.operator+(1);
			}

			// Rewinding to update lines & cols
			input.seekg(saved_pos);
			skip_to_pos(input, new_pos, current_line, line, col);
		}

		return matches;
	}

	string toString()
	{
		string str;
		for (RegexOperator* op : *this) {
			str += op->toString() + ", ";
		}
		return str;
	}
};

class RegexFactory
{
public:
	static RegexOperator* create_next_op(istream& input, Regex& regex, int& cols)
	{
		// TODO: implement OR
		int c = input.get();
		cols++;
		switch (c) {
		case '\\':
		{
			int antislash_command = input.get();
			cols++;
			switch (antislash_command) {
			case 't':
				return new RegexBasicChar('\t');
			case 'n':
				return new RegexBasicChar('\n');
			case '\\':
				return new RegexBasicChar('\\');
			case '|':
				return new RegexBasicChar('|');
			case '.':
				return new RegexBasicChar('.');
			case EOF:
				throw RegexError("EOF Reached after '\\' char", -1, cols);
			default:
				throw RegexError(string("Unknown '\\") + (char)antislash_command + "' character (code: " + to_string(antislash_command) + ")", -1, cols);
			}
			break;
		}
		case '.':
			return new RegexAnyChar();
		case '*':
			return new RegexKleenStar();
		case '|':
		{
			if (regex.empty()) {
				throw RegexError("no option found before | operator", -1, cols);
			}

			// Remove last operator & replace with OR
			auto or_op = new RegexOr();
			or_op->options.push_back(regex.back());
			regex.pop_back();

			RegexOperator* op;
			do {
				op = create_next_op(input, regex, cols);

				if (op != nullptr) {
					or_op->options.push_back(op);
				}
			}
			while (op != nullptr && input.peek() == '|' && input.get());

			if (or_op->options.size() == 1) {
				throw RegexError("no option found after | operator", -1, cols);
			}

			return or_op;
		}
		case EOF:
			return nullptr;
		default:
			return new RegexBasicChar(c);
		}
	}

	static Regex from_cstr(const char* str)
	{
		stringstream input(str);
		Regex regex;

		int cols = 0;
		RegexOperator* op = nullptr;
		do {
			op = create_next_op(input, regex, cols);

			if (op != nullptr) {
				regex.push_back(op);
			}
		}
		while (op != nullptr);

		if (regex.empty()) {
			throw RegexError("Empty regex", -1, cols);
		}

		cout << "Raw: '" << str << "'" << endl;
		cout << "Pattern: " << regex.toString() << endl;

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

	// Compile & search the regex
	list<Match> matches;
	auto t0 = high_resolution_clock::now();
	try {
		Regex regex = RegexFactory::from_cstr(argv[2]);
		matches = regex.search_in(input);
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
