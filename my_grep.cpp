#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <list>

using namespace std;

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

class InputFacade
{
	istream* input;

	string current_line;

	int lines_read = 0;
	int chars_since_line_start = 0;

	int last_line_chars = 0;

	int result = 0;

public:
	InputFacade(istream* _input)
	{
		input = _input;
	}

	int peek()
	{
		return input->peek();
	}

	bool eof()
	{
		return input->eof();
	}

	int get_lines_read()
	{
		int lines, chars;
		get_pos(lines, chars);
		return lines;
	}

	string get_line_start()
	{
		return current_line;
	}

	streampos tellg()
	{
		return input->tellg();
	}

	void seekg(streampos& pos)
	{
		input->seekg(pos);
	}

	string peek_line_end()
	{
		string line_end;
		streampos pos = input->tellg(); // Save pos

		getline(*input, line_end);

		input->seekg(pos); // Restore pos
		return line_end;
	}

	// Set line & chars with the current reading head position
	void get_pos(int& line, int& chars)
	{
		line = lines_read + 1;
		chars = chars_since_line_start;

		if (chars == 0) {
			line--;
			chars = last_line_chars;
		}
	}

	// Prints current pos & char
	void debug_pos(ostream& output)
	{
		int lines, chars;
		get_pos(lines, chars);
		output << "[" << lines << ", " << chars << "]: ";

		if (result == EOF) {
			output << "EOF" << endl;
		}
		else {
			output << i_to_string(result) << " (" << result << ")" << endl;
		}
	}

	// Get next char
	int get()
	{
		result = input->get();

		// debug_pos(cout);

		if (result == EOF) {
			return result;
		}

		if (result == '\n') {
			lines_read++;
			last_line_chars = chars_since_line_start + 1;
			chars_since_line_start = 0;
			current_line.clear();
		}
		else {
			chars_since_line_start++;
			current_line.push_back((char)result);
		}

		return result;
	}

	// Get until delimiter or EOF is reached
	string get(char delimiter)
	{
		string buffer;

		int result;
		while ((result = get()) != EOF && result != delimiter) {
			buffer.push_back((char)result);
		}

		return buffer;
	}
};

class RegexError : std::exception
{
public:
	string reason;
	int line_number;
	int char_number;

	RegexError(string e, InputFacade& input)
	{
		reason = e;
		input.get_pos(line_number, char_number);
	}

	string what()
	{
		return reason + " at (" + to_string(line_number) + ", " + to_string(char_number) + ")";
	}
};

class Match
{
	bool success;
	int line_start_number;

	string line_start;
	string line_end;

public:
	stringstream data;

	Match(string current_line, int line)
	{
		line_start = current_line;
		line_start_number = line;
	}

	Match(const Match& m)
	{
		success = m.success;
		line_start_number = m.line_start_number;
		line_start = m.line_start;
		line_end = m.line_end;
		data = stringstream(m.data.str());
	}

	void abort()
	{
		success = false;
	}

	void end(string _line_end)
	{
		line_end = _line_end;
		success = true;
	}

	bool is_success()
	{
		return success;
	}

	void print_line(ostream& output, int nb)
	{
		output << nb << "\t| ";
	}

	void print(ostream& output)
	{
		int line_nb = line_start_number;
		stringstream copy(data.str());

		// output << "Status: " << (success ? "Success" : "Aborted") << endl;

		print_line(output, line_nb);
		output << line_start << "\e[44m";

		while (copy.peek() != EOF) {
			char c = copy.get();

			if (c == '\n') {
				line_nb++;
				output << "\\n\e[0m" << endl;
				print_line(output, line_nb);
				output << "\e[44m";
			}
			else {
				output << c;
			}
		}

		output << "\e[0m" << line_end << endl;
	}
};

class RegexOperator
{
protected:
public:
	virtual bool execute(InputFacade& input, ostream& output, RegexOperator* next) = 0;
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

	virtual bool execute(InputFacade& input, ostream& output, RegexOperator* next)
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
	virtual bool execute(InputFacade& input, ostream& output, RegexOperator* next)
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
	virtual bool execute(InputFacade& input, ostream& output, RegexOperator* next)
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

	virtual bool execute(InputFacade& input, ostream& output, RegexOperator* next)
	{
		streampos pos;
		stringstream _output;
		bool success;

		// Test all options
		for (RegexOperator* option : options) {
			pos = input.tellg(); // Save pos
			_output.clear();
			success = option->execute(input, _output, next);
			input.seekg(pos); // Restore pos

			// If one option successfull
			if (success) {
				output << _output.str();
				return true;
			}
		}

		// If all options failed
		return false;
	}

	virtual string toString()
	{
		string str("OR(");

		for (RegexOperator* option : options) {
			str += option->toString() + "| ";
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

	list<Match> search_in(InputFacade& input)
	{
		list<Match> matches;
		RegexOperator* current, * previous;
		bool success;

		while (input.peek() != EOF) {
			// Match start
			// cout << "\nMatch start" << endl;
			Match match(input.get_line_start(), input.get_lines_read());
			success = true;

			auto it = begin();
			while (success && it != end()) {
				// cout << "Next char: '" << i_to_string(input.peek()) << "'" << endl;
				success = (*it)->execute(input, match.data, *(++it));
				// cout << "Current: " << (current == nullptr ? "null" : current->toString()) << endl;
			}

			if (success) {
				// Match success
				// cout << "Match success" << endl;
				match.end(input.peek_line_end());
				matches.push_back(match);
			}
			else {
				// Match failure
				// cout << "Match failure" << endl;
				match.abort();
			}
		}

		return matches;
	}

	list<Match> search_in(istream& is)
	{
		InputFacade i(&is);
		return search_in(i);
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
	static RegexOperator* create_next_op(InputFacade& input)
	{
		// TODO: implement OR
		int c = input.get();
		switch (c) {
		case '\\':
		{
			int antislash_command = input.get();
			switch (antislash_command) {
			case 't':
				return new RegexBasicChar('\t');
			case 'n':
				return new RegexBasicChar('\n');
			case '\\':
				return new RegexBasicChar('\\');
			case '.':
				return new RegexBasicChar('.');
			case EOF:
				throw RegexError("EOF Reached after '\\' char", input);
			default:
				throw RegexError(string("Unknown '\\") + (char)antislash_command + "' character (code: " + to_string(antislash_command) + ")", input);
			}
			break;
		}
		case '.':
			return new RegexAnyChar();
		case '*':
			return new RegexKleenStar();
		case EOF:
			return nullptr;
		default:
			return new RegexBasicChar(c);
		}
	}

	static RegexOperator* create_next_op_wrapper(InputFacade& input, Regex& regex)
	{
		// cout << "Next char: '" << i_to_string(input.peek()) << "'" << endl;
		RegexOperator* op = create_next_op(input);
		// cout << "Created: " << (op == nullptr ? "null" : op->toString()) << endl;

		if (op != nullptr) {
			regex.push_back(op);
		}

		return op;
	}

	static Regex from_cstr(const char* str)
	{
		stringstream ss(str);
		InputFacade input(&ss);
		Regex regex;

		RegexOperator* op = create_next_op_wrapper(input, regex);
		while (op != nullptr) {
			op = create_next_op_wrapper(input, regex);
		}

		if (regex.empty()) {
			throw RegexError("Empty regex", input);
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
	try {
		Regex regex = RegexFactory::from_cstr(argv[2]);
		matches = regex.search_in(input);
	}
	catch (RegexError& e) {
		std::cerr << "Error: " << e.what() << endl;
		return -1;
	}

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
