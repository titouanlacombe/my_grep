#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <list>

using namespace std;

// Help to print special characters
string c_to_string(char c)
{
	switch (c) {
	case '\n':
		return string("\\n");

	case '\r':
		return string("\\r");

	case '\t':
		return string("\\t");

	default:
		return string(1, c);
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
			output << c_to_string(result) << " (" << result << ")" << endl;
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

	RegexError(string e, InputFacade input)
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

	void add(string& matched)
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

	void print()
	{
		// cout << "Status: " << (success ? "Success" : "Aborted") << endl;

		// Puts blue background on extracted
		stringstream lines(line_start + "\e[44m" + extracted + "\e[0m" + line_end);
		string line;

		int line_nb = line_start_number;
		while (!lines.eof()) {
			getline(lines, line);
			cout << line_nb << "\t| " << line << endl;
			line_nb++;
		}
	}
};

class RegexOperator
{
public:
	virtual string execute(InputFacade& ss) = 0;
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

	virtual string execute(InputFacade& ss)
	{
		char c = ss.get();

		if (c == character) {
			return string(1, c);
		}
		else {
			return string();
		}
	}

	virtual string toString()
	{
		return "{basic: " + c_to_string(character) + "}";
	}
};

class RegexAnyChar : public RegexOperator
{
public:
	RegexAnyChar()
	{}

	virtual string execute(InputFacade& ss)
	{
		return string(1, ss.get());
	}

	virtual string toString()
	{
		return "{any char}";
	}
};

class Regex : public list<RegexOperator*>
{
public:
	Regex(string regex)
	{
		stringstream pattern(regex);
		InputFacade pattern_f(&pattern);

		int result;
		char c;
		while ((result = pattern_f.get()) != EOF) {
			c = result;

			if (c == '\\') {
				int next = pattern_f.get();

				switch (next) {
				case 't':
					push_back(new RegexBasicChar('\t'));
					break;

				case 'n':
					push_back(new RegexBasicChar('\n'));
					break;

				case '\\':
					push_back(new RegexBasicChar('\\'));
					break;

				case '.':
					push_back(new RegexBasicChar('.'));
					break;

				default:
					throw RegexError(string("Unknown '\\") + (char)next + "' character (code: " + to_string(next) + ")", pattern_f);
				}
			}
			else if (c == '.') {
				push_back(new RegexAnyChar());
			}
			else {
				push_back(new RegexBasicChar(c));
			}
		}

		// cout << "Encoded: " << regex << endl;
		// cout << "Pattern: ";
		// for (RegexOperator* v : *this) {
		// 	cout << v->toString() << ", ";
		// }
		// cout << endl;
	}

	list<Match> execute(InputFacade& ss)
	{
		list<Match> matches;

		while (!ss.eof()) {
			// Start the regex
			auto it = begin();
			Match current_match(ss.get_line_start(), ss.get_lines_read());

			// cout << "Match starting: ";
			// ss.debug_pos(cout);

			// Matches
			string matched;
			do {
				// cout << "Cheking " << (*it)->toString() << " at '" << c_to_string(ss.peek());

				matched = (*it)->execute(ss);

				// cout << "' Matched: '" << matched << "'" << endl;

				current_match.add(matched);
				it++;
			}
			while (!matched.empty() && !ss.eof() && it != end());

			// If reached end of regex add match to results
			if (it == end()) {
				current_match.end(ss.peek_line_end());
			}
			else {
				current_match.abort();
			}

			matches.push_back(current_match);
		}

		return matches;
	}

	list<Match> execute(istream& is)
	{
		InputFacade i(&is);
		return execute(i);
	}
};

class RegexOr : public Regex
{
	Regex* left;
	Regex* right;

public:
	RegexOr(Regex* _left, Regex* _right)
		: Regex("")
	{
		left = _left;
		right = _right;
	}

	virtual list<Match> execute(InputFacade& ss)
	{
		list<Match> a = left->execute(ss);
		list<Match> b = right->execute(ss);

		a.splice(a.end(), b); // Concatenate the two lists

		return a;
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
		Regex regex(argv[2]);
		matches = regex.execute(input);
	}
	catch (RegexError& e) {
		std::cerr << e.what() << endl;
		return -1;
	}

	// Prints the matches
	for (Match& m : matches) {
		if (m.is_success()) {
			m.print();
		}
	}

	input.close();

	return 0;
}
