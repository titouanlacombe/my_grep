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
void int_to_str(int result, string& output)
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
	case '\"':
		output.append("\\\"");
		return;
	case '\'':
		output.append("\\\'");
		return;
	case EOF:
		output.append("EOF");
		return;
	default:
		output.push_back(result);
		return;
	}
}

string int_to_str(int result)
{
	string str;
	int_to_str(result, str);
	return str;
}

string generate_json_str(string input)
{
	string str;

	for (char c : input) {
		int_to_str(c, str);
	}

	return "\"" + str + "\"";
}

string generate_json_key(string key)
{
	return generate_json_str(key) + ": ";
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
	Match(streampos& _start, streampos& _end)
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

// Abstract Node & Leaf of a regex tree
class AbstractRegexNode
{
public:
	virtual string toString() = 0;
	virtual void linearize(int ids_cache[CHAR_MAX]) = 0;
	virtual AbstractRegexNode* clean() = 0;
	virtual bool empty() = 0;
};

// Node of a regex tree wich is not a leaf
class RegexBranchNode : public AbstractRegexNode
{
public:
	// Operators are in order of appearance
	list<AbstractRegexNode*> childrens;

	~RegexBranchNode()
	{
		for (AbstractRegexNode* child : childrens) {
			delete child;
		}
	}

	void add_child(AbstractRegexNode* _child)
	{
		childrens.push_back(_child);
	}

	void add_childs(list<AbstractRegexNode*> _childs)
	{
		for (AbstractRegexNode* child : _childs) {
			add_child(child);
		}
	}

	bool empty()
	{
		return childrens.empty();
	}

	virtual string toString()
	{
		string str = "[\n";
		for (AbstractRegexNode* child : childrens) {
			str += child->toString();
			if (child != childrens.back()) {
				str += ",";
			}
			str += "\n";
		}
		return str + "]";
	}

	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		for (AbstractRegexNode* child : childrens) {
			child->linearize(ids_cache);
		}
	}

	virtual AbstractRegexNode* clean()
	{
		for (AbstractRegexNode*& child : childrens) {
			child = child->clean();
		}

		// filter out null childs
		auto it = childrens.begin();
		while (it != childrens.end()) {
			if (*it == nullptr) {
				it = childrens.erase(it);
			}
			else {
				it++;
			}
		}


		if (childrens.size() > 1) {
			return this;
		}

		if (childrens.size() == 0) {
			delete this;
			return nullptr;
		}

		AbstractRegexNode* child = childrens.front();
		childrens.clear();
		delete this;
		return child;
	}
};

// Node of a regex tree wich is a leaf
class RegexLeafNode : public AbstractRegexNode
{
public:
	virtual string toString() = 0;
	virtual void linearize(int ids_cache[CHAR_MAX]) = 0;

	bool empty()
	{
		return false;
	}

	virtual AbstractRegexNode* clean()
	{
		return this;
	}
};

// Node wich will match a specific character
class CharLeaf : public RegexLeafNode
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
		return generate_json_key("Char") + generate_json_str(string(1, character));
	}

	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		glushkov_id = ids_cache[character]++;
		// cout << "(letter: " << character << ", id: " << glushkov_id << "), ";
	}
};

// Node wich only match any character
class AnyLeaf : public RegexLeafNode
{
public:
	virtual string toString()
	{
		return generate_json_str("Any char");
	}

	virtual void linearize(int ids_cache[CHAR_MAX])
	{}
};

// Node wich match any number of character
class KleenStarOperator : public RegexLeafNode
{
public:
	RegexBranchNode* child;

	virtual string toString()
	{
		return generate_json_key("Kleen star") + child->toString();
	}

	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		child->linearize(ids_cache);
	}
};

// Node wich match one of the regex in it's childrens
class OrOperator : public RegexBranchNode
{
public:
	virtual string toString()
	{
		string str = generate_json_key("OR") + "[\n";

		for (AbstractRegexNode* option : childrens) {
			str += option->toString();
			if (option != childrens.back()) {
				str += ",";
			}
			str += "\n";
		}

		return str + "]";
	}

	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		for (AbstractRegexNode* option : childrens) {
			option->linearize(ids_cache);
		}
	}
};

class RegexFactory
{
public:
	// create spécial nodes after a '\'	char: \n \t etc...
	static RegexLeafNode* create_antislash_command(istream& input)
	{
		int c = input.get();
		cout << "Creating Antislash node '" << (char)c << "'" << endl;

		switch (c) {
		case 't':
			return new CharLeaf('\t');
		case 'n':
			return new CharLeaf('\n');
		case '\\':
			return new CharLeaf('\\');
		case '|':
			return new CharLeaf('|');
		case '\"':
			return new CharLeaf('\"');
		case '\'':
			return new CharLeaf('\'');
		case '(':
			return new CharLeaf('(');
		case ')':
			return new CharLeaf(')');
		case '*':
			return new CharLeaf('*');
		case '.':
			return new CharLeaf('.');
		case EOF:
			throw RegexError("EOF Reached after '\\' char", 0, input.tellg());
		default:
			throw RegexError(string("Unknown '\\") + (char)c + "' character (code: " + to_string(c) + ")", 0, input.tellg());
		}
	}

	static AbstractRegexNode* parse_leaf(istream& input)
	{
		int c = input.peek();
		cout << "parse_leaf: " << int_to_str(c) << endl;

		switch (c) {
		case EOF:
			return nullptr;
		case '|':
			return nullptr;
		case ')':
			return nullptr;
		case '(':
			input.get();
			return parse_or(input);
		case '\\':
			input.get();
			return create_antislash_command(input);
		default:
			input.get();
			return new CharLeaf(c);
		}
	}

	static RegexBranchNode* parse_branch(istream& input)
	{
		RegexBranchNode* branch = new RegexBranchNode();
		cout << "parse_branch" << endl;

		while (true) {
			AbstractRegexNode* leaf = parse_leaf(input);
			if (leaf != nullptr) {
				branch->add_child(leaf);
			}
			else {
				cout << "parse_branch exit" << endl;
				return branch;
			}
		}
	}

	static OrOperator* parse_or(istream& input)
	{
		OrOperator* _or = new OrOperator();
		cout << "parse_or" << endl;

		while (true) {
			_or->add_child(
				parse_branch(input)
			);

			int c = input.get();
			if (c != '|') {
				return _or;
			}
		}
	}

	static AbstractRegexNode* parse(istream& input)
	{
		RegexBranchNode* root = parse_or(input);

		ofstream json_log("./data/parsed.json");
		json_log << root->toString() << endl;

		// clean: collapse all branch with 1 child
		AbstractRegexNode* new_root = root->clean();

		if (new_root == nullptr) {
			throw RegexError("new_root null", 0, 0);
		}

		ofstream cleaned_json_log("./data/cleaned.json");
		cleaned_json_log << new_root->toString() << endl;

		return new_root;
	}

	static void linearize(AbstractRegexNode* regex_root)
	{
		int ids_cache[CHAR_MAX] = { 0 };

		regex_root->linearize(ids_cache);
	}

	static void glushkov(AbstractRegexNode* regex_root)
	{
		linearize(regex_root);
	}

	// Regex factory
	static AbstractRegexNode* from_cstr(const char* str)
	{
		stringstream input(str);

		// Parse the string regex into a node tree
		AbstractRegexNode* root = parse(input);

		if (root->empty()) {
			throw RegexError("Empty regex", 0, input.tellg());
		}

		// Convert the node tree into a compiled automaton
		glushkov(root);

		// cout << "Raw: '" << str << "'" << endl;

		return root;
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
		AbstractRegexNode* regex = RegexFactory::from_cstr(argv[2]);
		// matches = regex.search_in(input);
		delete regex;
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
