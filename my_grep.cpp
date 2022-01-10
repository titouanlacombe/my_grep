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
	string extracted;

public:
	Match(string current_line, int line)
	{
		line_start = current_line;
		line_start_number = line;
	}

	void add(string matched)
	{
		extracted += matched;
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
		stringstream extracted_ss(extracted);
		int line_nb = line_start_number;

		// output << "Status: " << (success ? "Success" : "Aborted") << endl;

		print_line(output, line_nb);
		output << line_start << "\e[44m";

		while (extracted_ss.peek() != EOF) {
			char c = extracted_ss.get();

			if (c == '\n') {
				line_nb++;
				output << "\e[0m" << endl;
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
	RegexOperator* next = nullptr;

public:
	virtual ~RegexOperator()
	{
		if (next != nullptr) {
			delete next;
		}
	}

	RegexOperator* append_regex(RegexOperator* _next)
	{
		next = _next;
		return next;
	}

	virtual string toString()
	{
		if (next == nullptr) {
			return string();
		}

		return ", " + next->toString();
	}

	virtual RegexOperator* execute(InputFacade& input, stringstream& output) = 0;
};

class RegexBasicChar : public RegexOperator
{
	char character;

public:
	RegexBasicChar(char c)
	{
		character = c;
	}

	virtual RegexOperator* execute(InputFacade& input, stringstream& output)
	{
		char c = input.get();

		if (c == character) {
			output << c;
			return next;
		}

		return nullptr;
	}

	virtual string toString()
	{
		return "Basic(" + i_to_string(character) + ")" + RegexOperator::toString();
	}
};

class RegexAnyChar : public RegexOperator
{
public:
	virtual RegexOperator* execute(InputFacade& input, stringstream& output)
	{
		output << input.get();
		return next;
	}

	virtual string toString()
	{
		return "Any()" + RegexOperator::toString();
	}
};

class RegexKleenStar : public RegexOperator
{
public:
	virtual RegexOperator* execute(InputFacade& input, stringstream& output)
	{
		RegexOperator* _next;
		streampos pos;
		stringstream _output;

		// While next is not valid add char to match
		do {
			pos = input.tellg(); // Save pos
			_output.clear();
			_next = next->execute(input, _output);
			input.seekg(pos); // Restore pos

			if (_next == nullptr) {
				// Add one char to match
				output << input.get();
			}
		}
		while (!input.eof() && _next == nullptr);

		// If next successfull add the matched str in output
		if (_next != nullptr) {
			output << _output.str();
		}

		return _next;
	}

	virtual string toString()
	{
		return "Star()" + RegexOperator::toString();
	}
};

class RegexOr : public RegexOperator
{
	list<RegexOperator*> options;

public:
	~RegexOr()
	{
		for (RegexOperator* option : options) {
			delete option;
		}
	}

	void add_option(RegexOperator* opt)
	{
		opt->append_regex(this);
		options.push_back(opt);
	}

	virtual RegexOperator* execute(InputFacade& input, stringstream& output)
	{
		RegexOperator* _next;
		streampos pos;
		stringstream _output;

		// Test all options
		for (RegexOperator* option : options) {
			pos = input.tellg(); // Save pos
			_output.clear();
			_next = option->execute(input, _output);
			input.seekg(pos); // Restore pos

			// If option successfull return the next and add the match in output
			if (_next != nullptr) {
				output << _output.str();
				return next;
			}
		}

		// If all options failed return null
		return nullptr;
	}

	virtual string toString()
	{
		string str("OR(");

		for (RegexOperator* option : options) {
			str += option->toString() + "| ";
		}

		return str + ")" + RegexOperator::toString();
	}
};

class Regex
{
	RegexOperator* first;
	RegexOperator* last;

public:

	Regex(RegexOperator* _first, RegexOperator* _last)
	{
		first = _first;
		last = _last;
	}

	~Regex()
	{
		delete first;
	}

	list<Match> execute(InputFacade& input)
	{
		list<Match> matches;
		stringstream output;
		RegexOperator* current = first;

		while (!input.eof()) {
			cout << "Searching match start" << endl;
			cout << "Next char: '" << i_to_string(input.peek()) << "'" << endl;
			current = current->execute(input, output);
			cout << "Current: " << (current == nullptr ? "null" : current->toString()) << endl;

			// Match not started
			if (current == nullptr) {
				continue;
			}

			// Match started
			cout << "Match started" << endl;
			Match match(input.get_line_start(), input.get_lines_read());

			while (current != nullptr && current != last) {
				cout << "Next char: '" << i_to_string(input.peek()) << "'" << endl;
				current = current->execute(input, output);
				cout << "Current: " << (current == nullptr ? "null" : current->toString()) << endl;
			}

			// Match add matched
			match.add(output.str());

			if (current == last) {
				// Match success
				cout << "Match success" << endl;
				match.end(input.peek_line_end());
				matches.push_back(match);
			}
			else {
				// Match failure
				cout << "Match failure" << endl;
				match.abort();
			}
		}

		return matches;
	}

	list<Match> execute(istream& is)
	{
		InputFacade i(&is);
		return execute(i);
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

	static Regex from_cstr(const char* str)
	{
		stringstream ss(str);
		InputFacade input(&ss);

		// cout << "Next char: '" << i_to_string(input.peek()) << "'" << endl;
		RegexOperator* first = create_next_op(input);
		// cout << "Created: " << (op == nullptr ? "null" : op->toString()) << endl;

		RegexOperator* current = first;
		while (current != nullptr) {
			// cout << "Next char: '" << i_to_string(input.peek()) << "'" << endl;
			RegexOperator* op = create_next_op(input);
			// cout << "Created: " << (op == nullptr ? "null" : op->toString()) << endl;

			current = current->append_regex(op);
		}

		cout << "Raw: '" << str << "'" << endl;
		cout << "Pattern: " << first->toString() << endl;

		return Regex(first, current);
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

	// Compile & execute the regex
	list<Match> matches;
	try {
		Regex regex = RegexFactory::from_cstr(argv[2]);
		matches = regex.execute(input);
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
