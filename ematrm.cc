#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <stack>
#include <vector>

#define REG_STR_SIZE 38

enum token_type {
	// atoms.
	TT_LIT_STR = 0,
	TT_LIT_CH,
	TT_LIT_NUM,
	
	// register mask settings.
	TT_TOGGLE_BIT_0,
	TT_TOGGLE_BIT_1,
	TT_TOGGLE_BIT_2,
	TT_TOGGLE_BIT_3,
	TT_TOGGLE_BIT_4,
	TT_TOGGLE_BIT_5,
	TT_TOGGLE_BIT_6,
	TT_TOGGLE_BIT_7,
	TT_TOGGLE_BIT_8,
	TT_TOGGLE_BIT_9,
	TT_TOGGLE_BIT_A,
	TT_TOGGLE_BIT_B,
	TT_TOGGLE_BIT_C,
	TT_TOGGLE_BIT_D,
	TT_TOGGLE_BIT_E,
	TT_TOGGLE_BIT_F,
	TT_TOGGLE_COL_0,
	TT_TOGGLE_COL_1,
	TT_TOGGLE_COL_2,
	TT_TOGGLE_COL_3,
	TT_TOGGLE_ROW_0,
	TT_TOGGLE_ROW_1,
	TT_TOGGLE_ROW_2,
	TT_TOGGLE_ROW_3,
	TT_TOGGLE_MAT,
	
	// operation mode settings.
	TT_OP_MODE_COL,
	TT_OP_MODE_ROW,
	TT_OP_ORDER_REV,
	
	// operations.
	TT_POP_ATOM,
	TT_PUSH_ATOM,
	TT_WRITE_STDOUT,
	TT_WRITE_STDOUT_NEWLINE,
	TT_READ_STDIN,
	TT_STR_TO_INT,
	TT_INT_TO_STR,
	TT_ADD,
	TT_SUB,
	TT_MUL,
	TT_DIV,
	TT_NUM_ADD,
	TT_NUM_SUB,
	TT_NUM_MUL,
	TT_NUM_DIV,
	TT_IND_ADD,
	TT_IND_SUB,
	TT_IND_MUL,
	TT_IND_DIV,
	TT_POP_JMP,
	TT_POP_JMP_COND,
	TT_PUSH_JMP,
	TT_SAVE_JMP,
	TT_EQUAL,
	TT_GREQUAL,
	TT_GREATER,
	TT_LESS,
	TT_LEQUAL,
	TT_AND,
	TT_OR,
	TT_NOT,
};

enum op_mode {
	OM_ROW = 0,
	OM_COL,
};

enum reg_type {
	RT_STR = 0,
	RT_INT,
};

struct token {
	token_type type;
	std::string data;
	long line;
};

struct reg {
	reg_type type;
	union {
		char str[REG_STR_SIZE];
		long num;
	} data;
};

struct machine {
	reg regs[16];
	
	uint16_t mask;
	size_t instr_ptr;
	op_mode mode;
	bool rev;
	
	std::stack<token> atoms;
	std::stack<long> jumps;
};

static void err(std::string const &msg);
static void prog_err(unsigned line, std::string const &msg);
static std::string read_file(std::ifstream &f);
static std::optional<token> lex_string(std::string const &src, size_t &i, unsigned &line);
static std::optional<token> lex_char(std::string const &src, size_t &i, unsigned &line);
static std::optional<token> lex_num(std::string const &src, size_t &i, unsigned &line);
static std::optional<std::vector<token>> lex(std::string const &src);
static void for_each_reg(machine &machine, std::function<void(reg &)> const &fn);
static void exec_cycle(machine &machine, std::vector<token> const &code);

int
main(int argc, char const *argv[])
{
	if (argc != 2) {
		std::cerr << "usage: " << argv[0] << " <file>\n";
		return 1;
	}
	
	std::ifstream f{argv[1], std::ios::binary};
	if (!f) {
		err("failed to open file!");
		return 1;
	}
	
	std::string src = read_file(f);
	std::optional<std::vector<token>> code = lex(src);
	if (!code) {
		err("failed to lex file!");
		return 1;
	}
	
	machine machine = {
		.mask = 0x0,
		.instr_ptr = 0,
		.mode = OM_ROW,
		.rev = false,
		.atoms = std::stack<token>{},
		.jumps = std::stack<long>{},
	};
	while (machine.instr_ptr < code->size())
		exec_cycle(machine, *code);
	
	return 0;
}

static void
err(std::string const &msg)
{
	std::cerr << "err: " << msg << '\n';
}

static void
prog_err(unsigned line, std::string const &msg)
{
	std::cerr << '[' << line << "] err: " << msg << '\n';
}

static std::string
read_file(std::ifstream &f)
{
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static std::optional<token>
lex_string(std::string const &src, size_t &i, unsigned &line)
{
	unsigned line_start = line;
	std::string data = "";
	
	while (i < src.length()) {
		if (src[i] == '\n')
			++line;
		else if (src[i] == '"')
			break;
		data += src[i];
		++i;
	}
	
	if (i == src.length()) {
		prog_err(line_start, "unterminated string!");
		return std::nullopt;
	}
	
	token tok = {
		.type = TT_LIT_STR,
		.data = data,
		.line = line_start,
	};
	
	return tok;
}

static std::optional<token>
lex_char(std::string const &src, size_t &i, unsigned &line)
{
	if (i == src.length()) {
		prog_err(line, "non-existent character!");
		return std::nullopt;
	}
	
	token tok = {
		.type = TT_LIT_CH,
		.data = std::string{src[i]},
		.line = line,
	};
	
	if (src[i] == '\n')
		++line;
	
	return tok;
}

static std::optional<token>
lex_num(std::string const &src, size_t &i, unsigned &line)
{
	std::string data = "";
	
	while (i < src.length()) {
		if (src[i] == '$')
			break;
		else if (!isdigit(src[i])) {
			prog_err(line, "non-decimal-digit in number!");
			return std::nullopt;
		}
		data += src[i];
		++i;
	}
	
	if (i == src.length()) {
		prog_err(line, "unterminated number!");
		return std::nullopt;
	}
	
	token tok = {
		.type = TT_LIT_NUM,
		.data = data,
		.line = line,
	};
	
	return tok;
}

static std::optional<std::vector<token>>
lex(std::string const &src)
{
	std::vector<token> toks;
	unsigned line = 1;
	
	for (size_t i = 0; i < src.length(); ++i) {
		// skip whitespace.
		if (src[i] == '\n') {
			++line;
			continue;
		} else if (isspace(src[i]))
			continue;
		
		// handle literals.
		if (src[i] == '"') {
			std::optional<token> tok = lex_string(src, ++i, line);
			if (!tok)
				return std::nullopt;
			toks.push_back(*tok);
			continue;
		} else if (src[i] == '\'') {
			std::optional<token> tok = lex_char(src, ++i, line);
			if (!tok)
				return std::nullopt;
			toks.push_back(*tok);
			continue;
		} else if (src[i] == '$') {
			std::optional<token> tok = lex_num(src, ++i, line);
			if (!tok)
				return std::nullopt;
			toks.push_back(*tok);
			continue;
		}
		
		// handle register mask operations.
		if (src[i] >= '0' && src[i] <= '9') {
			token tok = {
				.type = static_cast<token_type>(TT_TOGGLE_BIT_0 + src[i] - '0'),
				.data = "",
				.line = line,
			};
			toks.push_back(tok);
			continue;
		} else if (src[i] >= 'a' && src[i] <= 'f') {
			token tok = {
				.type = static_cast<token_type>(TT_TOGGLE_BIT_A + src[i] - 'a'),
				.data = "",
				.line = line,
			};
			toks.push_back(tok);
			continue;
		} else if (src[i] == '|') {
			if (i++ == src.length() - 1) {
				prog_err(line, "expected column number after '|'!");
				return std::nullopt;
			}
			if (src[i] < '0' || src[i] > '3') {
				prog_err(line, "invalid column number!");
				return std::nullopt;
			}
			token tok = {
				.type = static_cast<token_type>(TT_TOGGLE_COL_0 + src[i] - '0'),
				.data = "",
				.line = line,
			};
			toks.push_back(tok);
			continue;
		} else if (src[i] == '`') {
			if (i++ == src.length() - 1) {
				prog_err(line, "expected row number after '`'!");
				return std::nullopt;
			}
			if (src[i] < '0' || src[i] > '3') {
				prog_err(line, "invalid row number!");
				return std::nullopt;
			}
			token tok = {
				.type = static_cast<token_type>(TT_TOGGLE_ROW_0 + src[i] - '0'),
				.data = "",
				.line = line,
			};
			toks.push_back(tok);
			continue;
		} else if (src[i] == 'A') {
			token tok = {
				.type = TT_TOGGLE_MAT,
				.data = "",
				.line = line,
			};
			toks.push_back(tok);
			continue;
		}
		
		token_type type;
		switch (src[i]) {
			// handle operator modes.
		case 'C':
			type = TT_OP_MODE_COL;
			break;
		case 'R':
			type = TT_OP_MODE_ROW;
			break;
		case '~':
			type = TT_OP_ORDER_REV;
			break;
			
			// handle operators.
		case '>':
			type = TT_POP_ATOM;
			break;
		case '<':
			type = TT_PUSH_ATOM;
			break;
		case 'w':
			type = TT_WRITE_STDOUT;
			break;
		case 'W':
			type = TT_WRITE_STDOUT_NEWLINE;
			break;
		case 'r':
			type = TT_READ_STDIN;
			break;
		case '#':
			type = TT_STR_TO_INT;
			break;
		case ',':
			type = TT_INT_TO_STR;
			break;
		case '+':
			type = TT_ADD;
			break;
		case '-':
			type = TT_SUB;
			break;
		case '*':
			type = TT_MUL;
			break;
		case '/':
			type = TT_DIV;
			break;
		case '%':
			if (++i == src.length()) {
				prog_err(line, "expected register number operator after '%'!");
				return std::nullopt;
			}
			switch (src[i]) {
			case '+':
				type = TT_NUM_ADD;
				break;
			case '-':
				type = TT_NUM_SUB;
				break;
			case '*':
				type = TT_NUM_MUL;
				break;
			case '/':
				type = TT_NUM_DIV;
				break;
			default:
				prog_err(line, "invalid register number operator!");
				return std::nullopt;
			}
			break;
		case '[':
			if (++i == src.length()) {
				prog_err(line, "expected register index operator after '['!");
				return std::nullopt;
			}
			switch (src[i]) {
			case '+':
				type = TT_IND_ADD;
				break;
			case '-':
				type = TT_IND_SUB;
				break;
			case '*':
				type = TT_IND_MUL;
				break;
			case '/':
				type = TT_IND_DIV;
				break;
			default:
				prog_err(line, "invalid register index operator!");
				return std::nullopt;
			}
			break;
		case 'j':
			if (++i == src.length()) {
				prog_err(line, "expected jump stack operator after 'j'!");
				return std::nullopt;
			}
			switch (src[i]) {
			case '>':
				type = TT_POP_JMP;
				break;
			case '?':
				type = TT_POP_JMP_COND;
				break;
			case '<':
				type = TT_PUSH_JMP;
				break;
			default:
				prog_err(line, "invalid jump stack operator!");
				return std::nullopt;
			}
			break;
		case '.':
			type = TT_SAVE_JMP;
			break;
		case '=':
			type = TT_EQUAL;
			break;
		case 'F':
			type = TT_GREQUAL;
			break;
		case 'G':
			type = TT_GREATER;
			break;
		case 'L':
			type = TT_LESS;
			break;
		case 'M':
			type = TT_LEQUAL;
			break;
		case '&':
			type = TT_AND;
			break;
		case '?':
			if (++i == src.length()) {
				prog_err(line, "expected '|' after '?'!");
				break;
			}
			switch (src[i]) {
			case '|':
				type = TT_OR;
				break;
			default:
				prog_err(line, "invalid boolean operator!");
				return std::nullopt;
			}
			break;
		case '!':
			type = TT_NOT;
			break;
			
		default:
			prog_err(line, "unknown character!");
			return std::nullopt;
		}
		
		token tok = {
			.type = type,
			.data = "",
			.line = line,
		};
		toks.push_back(tok);
	}
	
	return toks;
}

static void
for_each_reg(machine &machine, std::function<void(reg &, size_t)> const &fn)
{
	auto maybe_apply = [&](int ind) {
		if (machine.mask & 1 << ind)
			fn(machine.regs[ind], ind);
	};
	
	if (!machine.rev) {
		if (machine.mode == OM_ROW) {
			for (int i = 0; i < 4; ++i) {
				maybe_apply(4 * i);
				maybe_apply(4 * i + 1);
				maybe_apply(4 * i + 2);
				maybe_apply(4 * i + 3);
			}
		} else {
			for (int i = 0; i < 4; ++i) {
				maybe_apply(i);
				maybe_apply(i + 4);
				maybe_apply(i + 8);
				maybe_apply(i + 12);
			}
		}
	} else {
		if (machine.mode == OM_ROW) {
			for (int i = 0; i < 4; ++i) {
				maybe_apply(4 * i + 3);
				maybe_apply(4 * i + 2);
				maybe_apply(4 * i + 1);
				maybe_apply(4 * i);
			}
		} else {
			for (int i = 0; i < 4; ++i) {
				maybe_apply(i + 12);
				maybe_apply(i + 8);
				maybe_apply(i + 4);
				maybe_apply(i);
			}
		}
	}
}

static void
exec_cycle(machine &machine, std::vector<token> const &code)
{
	token const &tok = code[machine.instr_ptr++];
	
	// handle bit toggling since these can easily be implemented without
	// having to do the ugly thing of defining 25 parallel switch cases for
	// every possibility.
	if (tok.type >= TT_TOGGLE_BIT_0 && tok.type <= TT_TOGGLE_BIT_F) {
		machine.mask ^= 1 << static_cast<int>(tok.type - TT_TOGGLE_BIT_0);
		return;
	} else if (tok.type >= TT_TOGGLE_ROW_0 && tok.type <= TT_TOGGLE_ROW_3) {
		machine.mask ^= 0xf << 4 * static_cast<int>(tok.type - TT_TOGGLE_ROW_0);
		return;
	} else if (tok.type >= TT_TOGGLE_COL_0 && tok.type <= TT_TOGGLE_COL_3) {
		machine.mask ^= 0x1111 << static_cast<int>(tok.type - TT_TOGGLE_COL_0);
		return;
	} else if (tok.type == TT_TOGGLE_MAT) {
		machine.mask ^= 0xffff;
		return;
	}
	
	switch (tok.type) {
		// handle atoms.
	case TT_LIT_STR:
	case TT_LIT_CH:
	case TT_LIT_NUM:
		machine.atoms.push(tok);
		break;
		
		// handle operator modes.
	case TT_OP_MODE_COL:
		machine.mode = OM_COL;
		break;
	case TT_OP_MODE_ROW:
		machine.mode = OM_ROW;
		break;
	case TT_OP_ORDER_REV:
		machine.rev = !machine.rev;
		break;
		
		// handle operators.
	case TT_POP_ATOM: {
		if (!machine.atoms.size())
			break;
		
		token tok = machine.atoms.top();
		machine.atoms.pop();
		if (tok.type == TT_LIT_STR) {
			auto pop = [&](reg &reg, size_t num) {
				reg.type = RT_STR;
				strncpy(reg.data.str, tok.data.c_str(), REG_STR_SIZE);
			};
			for_each_reg(machine, pop);
		} else if (tok.type == TT_LIT_CH) {
			auto pop = [&](reg &reg, size_t num) {
				reg.type = RT_INT;
				reg.data.num = tok.data[0];
			};
			for_each_reg(machine, pop);
		} else {
			auto pop = [&](reg &reg, size_t num) {
				reg.type = RT_INT;
				reg.data.num = atoi(tok.data.c_str());
			};
			for_each_reg(machine, pop);
		}
		
		break;
	}
	case TT_PUSH_ATOM: {
		auto push = [&](reg &reg, size_t num) {
			token tok = {
				// special value used for non-lexed atom tokens.
				.line = -1,
			};
			
			if (reg.type == RT_INT) {
				tok.type = TT_LIT_NUM;
				tok.data = std::to_string(reg.data.num);
			} else {
				tok.type = TT_LIT_STR;
				char buf[REG_STR_SIZE + 1] = {0};
				strcpy(buf, reg.data.str);
				tok.data = std::string{buf};
			}
			
			machine.atoms.push(tok);
		};
		for_each_reg(machine, push);
		break;
	}
	case TT_WRITE_STDOUT: {
		auto write = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				std::cout << reg.data.num;
			else {
				char buf[REG_STR_SIZE + 1] = {0};
				strcpy(buf, reg.data.str);
				std::cout << buf;
			}
		};
		for_each_reg(machine, write);
		break;
	}
	case TT_WRITE_STDOUT_NEWLINE: {
		auto write = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				std::cout << reg.data.num << '\n';
			else {
				char buf[REG_STR_SIZE + 1] = {0};
				strcpy(buf, reg.data.str);
				std::cout << buf << '\n';
			}
		};
		for_each_reg(machine, write);
		break;
	}
	case TT_READ_STDIN: {
		std::string input;
		std::cout << ">: ";
		std::cin >> input;
		
		auto write_input = [&](reg &reg, size_t num) {
			strncpy(reg.data.str, input.c_str(), REG_STR_SIZE);
			reg.type = RT_STR;
		};
		
		for_each_reg(machine, write_input);
		
		break;
	}
	case TT_STR_TO_INT: {
		auto str_to_int = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				return;
			
			char buf[REG_STR_SIZE + 1] = {0};
			strcpy(buf, reg.data.str);
			reg.data.num = atoi(buf);
			reg.type = RT_INT;
		};
		for_each_reg(machine, str_to_int);
		break;
	}
	case TT_INT_TO_STR: {
		auto int_to_str = [&](reg &reg, size_t num) {
			if (reg.type == RT_STR)
				return;
			
			std::string str = std::to_string(reg.data.num);
			strncpy(reg.data.str, str.c_str(), REG_STR_SIZE);
			reg.type = RT_STR;
		};
		for_each_reg(machine, int_to_str);
		break;
	}
	case TT_ADD: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		auto add = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num += val;
		};
		
		for_each_reg(machine, add);
		
		break;
	}
	case TT_SUB: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		auto sub = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num -= val;
		};
		
		for_each_reg(machine, sub);
		
		break;
	}
	case TT_MUL: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		auto mul = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num *= val;
		};
		
		for_each_reg(machine, mul);
		
		break;
	}
	case TT_DIV: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		// division by zero not allowed.
		if (val == 0)
			break;
		
		auto div = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num /= val;
		};
		
		for_each_reg(machine, div);
		
		break;
	}
	case TT_NUM_ADD: {
		auto num_add = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num += num;
		};
		for_each_reg(machine, num_add);
		break;
	}
	case TT_NUM_SUB: {
		auto num_sub = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num -= num;
		};
		for_each_reg(machine, num_sub);
		break;
	}
	case TT_NUM_MUL: {
		auto num_mul = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num *= num;
		};
		for_each_reg(machine, num_mul);
		break;
	}
	case TT_NUM_DIV: {
		auto num_div = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT && num > 0)
				reg.data.num /= num;
		};
		for_each_reg(machine, num_div);
		break;
	}
	case TT_IND_ADD: {
		size_t ind = 0;
		auto ind_add = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num += ind;
			++ind;
		};
		for_each_reg(machine, ind_add);
		break;
	}
	case TT_IND_SUB: {
		size_t ind = 0;
		auto ind_sub = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num -= ind;
			++ind;
		};
		for_each_reg(machine, ind_sub);
		break;
	}
	case TT_IND_MUL: {
		size_t ind = 0;
		auto ind_mul = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num *= ind;
			++ind;
		};
		for_each_reg(machine, ind_mul);
		break;
	}
	case TT_IND_DIV: {
		size_t ind = 0;
		auto ind_div = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT && ind > 0)
				reg.data.num /= ind;
			++ind;
		};
		for_each_reg(machine, ind_div);
		break;
	}
	case TT_POP_JMP: {
		if (!machine.jumps.size())
			break;
		
		long jmp = machine.jumps.top();
		machine.jumps.pop();
		if (jmp < 0 || jmp >= code.size())
			break;
		
		machine.instr_ptr = jmp;
		
		break;
	}
	case TT_POP_JMP_COND: {
		if (!machine.jumps.size() || !machine.atoms.size())
			break;
		
		long jmp = machine.jumps.top();
		long cond = atoi(machine.atoms.top().data.c_str());
		machine.jumps.pop();
		machine.atoms.pop();
		if (jmp < 0 || jmp >= code.size())
			break;
		
		if (cond)
			machine.instr_ptr = jmp;
		
		break;
	}
	case TT_PUSH_JMP: {
		auto push_jmp = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				machine.jumps.push(reg.data.num);
		};
		for_each_reg(machine, push_jmp);
		break;
	}
	case TT_SAVE_JMP:
		// needs to be subtracted by 1 in order do account for `++`.
		machine.jumps.push(machine.instr_ptr - 1);
		break;
	case TT_EQUAL: {
		if (!machine.atoms.size())
			break;
		
		token tok = machine.atoms.top();
		machine.atoms.pop();
		
		auto equal = [&](reg &reg, size_t num) {
			if (tok.type == TT_LIT_NUM && reg.type == RT_INT) {
				long val = atoi(tok.data.c_str());
				reg.data.num = reg.data.num == val;
			} else if (tok.type == TT_LIT_STR && reg.type == RT_STR) {
				char buf[REG_STR_SIZE + 1] = {0};
				strncpy(buf, reg.data.str, REG_STR_SIZE);
				reg.type = RT_INT;
				reg.data.num = !strcmp(buf, tok.data.c_str());
			}
		};
		
		for_each_reg(machine, equal);
		
		break;
	}
	case TT_GREQUAL: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		auto grequal = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num = reg.data.num >= val;
		};
		
		for_each_reg(machine, grequal);
		
		break;
	}
	case TT_GREATER: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		auto greater = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num = reg.data.num > val;
		};
		
		for_each_reg(machine, greater);
		
		break;
	}
	case TT_LESS: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		auto less = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num = reg.data.num < val;
		};
		
		for_each_reg(machine, less);
		
		break;
	}
	case TT_LEQUAL: {
		if (!machine.atoms.size())
			break;
		
		long val = atoi(machine.atoms.top().data.c_str());
		machine.atoms.pop();
		
		auto lequal = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num = reg.data.num <= val;
		};
		
		for_each_reg(machine, lequal);
		
		break;
	}
	case TT_AND: {
		bool all_set = true;
		auto and_ = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT && !reg.data.num)
				all_set = false;
		};
		for_each_reg(machine, and_);
		
		token tok = {
			.type = TT_LIT_NUM,
			.data = all_set ? "1" : "0",
			.line = -1,
		};
		machine.atoms.push(tok);
		
		break;
	}
	case TT_OR: {
		bool any_set = false;
		auto or_ = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT && reg.data.num)
				any_set = true;
		};
		for_each_reg(machine, or_);
		
		token tok = {
			.type = TT_LIT_NUM,
			.data = any_set ? "1" : "0",
			.line = -1,
		};
		machine.atoms.push(tok);
		
		break;
	}
	case TT_NOT: {
		auto not_ = [&](reg &reg, size_t num) {
			if (reg.type == RT_INT)
				reg.data.num = !reg.data.num;
		};
		for_each_reg(machine, not_);
		break;
	}
		
	default:
		break;
	}
}
