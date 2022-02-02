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

// Abstract Node & Leaf of a regex tree
class AbstractRegexNode
{
public:
	AbstractRegexNode* parent = nullptr;

	virtual string toString() = 0;
	virtual void linearize(int ids_cache[CHAR_MAX]) = 0;
};

// Node of a regex tree wich is not a leaf
class RegexBranchNode : public AbstractRegexNode
{
public:
	// Operators are in order of appearance
	list<AbstractRegexNode*> childrens;

	~RegexBranchNode()
	{
		for (AbstractRegexNode *op : childrens) {
			delete op;
		}
	}
	
	void add_child(AbstractRegexNode *_child)
	{
		_child->parent = this;
		childrens.push_back(_child);
	}

	bool empty()
	{
		return childrens.empty();
	}

	virtual string toString()
	{
		string str = "TreeNode{";
		for (AbstractRegexNode *op : childrens) {
			str += op->toString() + ", ";
		}
		return str + "}, ";
	}
	
	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		for (AbstractRegexNode *op : childrens) {
			op->linearize(ids_cache);
		}
	}
};

// Node of a regex tree wich is a leaf
class RegexLeafNode : public AbstractRegexNode
{
public:
	virtual string toString() = 0;
	virtual void linearize(int ids_cache[CHAR_MAX]) = 0;
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

// Node wich only match any character
class AnyLeaf : public RegexLeafNode
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

// Node wich match any number of character
class KleenStarLeaf : public RegexLeafNode
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

// Node wich match one of the regex in it's childrens
class OrNode : public RegexBranchNode
{
public:
	virtual string toString()
	{
		string str("ORNode[");

		for (AbstractRegexNode *option : childrens) {
			str += option->toString() + ", ";
		}

		return str + "]";
	}
	
	virtual void linearize(int ids_cache[CHAR_MAX])
	{
		for (AbstractRegexNode *option : childrens) {
			option->linearize(ids_cache);
		}
	}
};

class RegexFactory
{
public:
	// create spécial nodes after a '\'	char: \n \t etc...
	static AbstractRegexNode *create_antislach_command(istream &input)
	{
		int c = input.get();

		switch (c) {
		case 't':
			return new CharLeaf('\t');
		case 'n':
			return new CharLeaf('\n');
		case '\\':
			return new CharLeaf('\\');
		case '|':
			return new CharLeaf('|');
		case '(':
			return new CharLeaf('(');
		case ')':
			return new CharLeaf(')');
		case '*':
			return new CharLeaf('*');
		case '.':
			return new CharLeaf('.');
		case EOF:
			throw RegexError("EOF Reached after '\\' char", 0, 0);
		default:
			throw RegexError(string("Unknown '\\") + (char)c + "' character (code: " + to_string(c) + ")", 0, 0);
		}
	}
	
	// Create an OrNode (convert parent to OrNode)
	static AbstractRegexNode *create_or_node(istream &input, RegexBranchNode *parent)
	{
		OrNode *or_node = new OrNode();

		// Copy childrens of parent
		or_node->childrens = parent->childrens;

		// Clear childrens of parent so that it doesn't free on destruct
		parent->childrens.clear();
		delete parent;

		// Replace parent->parent->last_child by OrNode
		if (parent->parent == nullptr) {
			throw RegexError("parent->parent is null, dev error, add a root node", 0, 0);
		}
		AbstractRegexNode *&child_ref = ((RegexBranchNode*)parent->parent)->childrens.back();
		if (child_ref == nullptr) {
			throw RegexError("no previous node was found at create OrNode", 0, 0);
		}
		child_ref = or_node;

		// Add next options
		do {
			// Add branch wrapper to create new node(s)
			// Simulate the presence of parenthesis at each '|' char (like: "(blable)|(bnieh)|(vrueb)")
			RegexBranchNode *option = new RegexBranchNode();
			
			create_nodes(input, option);

			if (option->empty()) {
				throw RegexError("reached EOF after | char", 0, 0);
			}
			
			or_node->add_child(option);
		}
		while (input.peek() == '|' && input.get());
		
		return or_node;
	}
	
	static AbstractRegexNode *create_star_node(istream &input, RegexBranchNode *parent)
	{
		return nullptr;
	}

	static AbstractRegexNode *create_node(istream &input, RegexBranchNode *parent)
	{
		int c = input.get();

		switch (c)
		{
		case EOF:
			return nullptr;
		case '\\':
			return create_antislach_command(input);
		case '|':
			return create_or_node(input, parent);
		case '*':
			return create_star_node(input, parent);
		case '(':
		{
			RegexBranchNode *new_branch = new RegexBranchNode();
			create_nodes(input, new_branch);
			return new_branch;
		}
		case ')':
			return nullptr;
		default:
			return new CharLeaf(c);
		}
	}
	
	static void create_nodes(istream &input, RegexBranchNode *parent)
	{
		if (parent == nullptr) {
			throw RegexError("null parent at create_nodes", 0, 0);
		}

		AbstractRegexNode *node;
		do {
			node = create_node(input, parent);

			if (node != nullptr) {
				parent->add_child(node);
			}
		}
		while (node != nullptr);
	}
	
	static RegexBranchNode parse(istream &input)
	{
		RegexBranchNode root;

		// Simulate the presence of parentheses at the beggining and end of the input
		// Used when the regex is an OR node => so the root isn't affected
		RegexBranchNode *main = new RegexBranchNode();
		root.add_child(main);
		
		create_nodes(input, main);
		
		cout << "Parsed: " << root.toString() << endl;

		return root;
	}

	static void linearize(RegexBranchNode &regex_root)
	{
		int ids_cache[CHAR_MAX] = {0};

		regex_root.linearize(ids_cache);
	}

	static void glushkov(RegexBranchNode &regex_root)
	{
		linearize(regex_root);
	}

	// Regex factory
	static RegexBranchNode from_cstr(const char* str)
	{
		stringstream input(str);

		// Parse the string regex into a node tree
		RegexBranchNode root = parse(input);

		if (root.empty()) {
			throw RegexError("Empty regex", 0, 0);
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
		RegexBranchNode regex = RegexFactory::from_cstr(argv[2]);
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
