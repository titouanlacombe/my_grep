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
	int lines_read = 0;
	int chars_since_line_start = 0;
	int last_line_chars = 0;
	int result = 0;

public:
	InputFacade(istream* _input)
	{
		input = _input;
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
		}
		else {
			chars_since_line_start++;
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

enum
{
	RegexCode_any = 300, // Matches any character
};

void print_found(InputFacade& input_f, ostream& output, string& extracted)
{
	output << "Pattern found at: ";
	input_f.debug_pos(output);
	output << "Extracted: " << extracted << endl;
}

bool regex_equal(char c, int reg_char)
{
	if (c == reg_char) {
		return true;
	}
	else if (reg_char == RegexCode_any) {
		return true;
	}
	else {
		return false;
	}
}

list<int> regex_decode(string regex)
{
	stringstream pattern(regex);
	InputFacade pattern_f(&pattern);

	list<int> decoded;

	int result;
	char c;
	while ((result = pattern_f.get()) != EOF) {
		c = result;

		if (c == '\\') {
			int next = pattern_f.get();

			switch (next) {
			case 't':
				decoded.push_back('\t');
				break;

			case 'n':
				decoded.push_back('\n');
				break;

			case '\\':
				decoded.push_back('\\');
				break;

			case '.':
				decoded.push_back('.');
				break;

			default:
				throw RegexError(string("Unknown '\\") + (char)next + "' character (code: " + to_string(next) + ")", pattern_f);
			}
		}
		else if (c == '.') {
			decoded.push_back(RegexCode_any);
		}
		else {
			decoded.push_back(c);
		}
	}

	// cout << "Encoded: " << regex << endl;
	// cout << "Pattern: ";
	// for (int v : decoded) {
	// 	cout << v << ", ";
	// }
	// cout << endl;

	return decoded;
}

void grep(istream& input, ostream& output, list<int> regex)
{
	// Search pattern
	InputFacade input_f(&input);

	string extracted;
	char c;
	int result;
	auto pattern_it = regex.begin();
	while ((result = input_f.get()) != EOF) {
		c = (char)result;

		if (regex_equal(c, *pattern_it)) {
			// Pattern success, searching next char
			extracted.push_back(c);
			pattern_it++;
		}
		else {
			// Pattern fail, restart pattern
			extracted.clear();
			pattern_it = regex.begin();
		}

		if (pattern_it == regex.end()) {
			// Found, print result & restart pattern
			print_found(input_f, output, extracted);
			extracted.clear();
			pattern_it = regex.begin();
		}
	}
}

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

	list<int> decoded_pattern;
	try {
		// Decode pattern
		decoded_pattern = regex_decode(argv[2]);
	}
	catch (RegexError& e) {
		std::cerr << e.what() << endl;
		return -1;
	}

	grep(input, cout, decoded_pattern);

	input.close();

	return 0;
}
