#include <cstdint>
#include <iostream>
#include <string>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>
#include <tuple>

/*
 * TODO:
 *  [x] setup builtin op dictionary
 *  [x] finish definition parsing
 *  [x] test sets
 *  [?] fix list parsing
 *  [x] char parsing
 *  [] add bool handling to ops
 *  [] add list/set handling to ops
 *  [] some ops and combinaors...
 *     [] and
 *     [] or
 *     [] not
 *     [] reverse
 *     [x] dip
 *     [] first
 *     [] rest
 *     [] at
 *     [] ifte
 *     [] step
 *     [] filter
 *     [x] fold
 *     [] cleave
 *     [] primrec
 *     [] linrec
 *     [] split
 *     [] uncons
 *     [] small 
 *     [] binrec
 *     [] pred
 *     [] treerec
 *     [] size
 *     [] powerlist
 *     [] swons
 *     [x] x
 *     ... and many more
 *  [x] fix type checking (should happen before popping the values off somehow?)
 *  [] add block (DEFINITION, LIBRA) parsing
 *  [] error handling
 *  [] garbage collection ? (joy doesn't really use variables in the same way as most
       langs, so this might not be necessary. we just have to clean the stack out
	   when necessary)
 *  [] plan out recursive combinators
 */

#define STACK_SIZE 1000

typedef void (*voidFunction)(void); 

enum Type { BOOL, INT, STR, FLOAT, LIST, SET, OP, CHAR };

struct joy_object;

typedef std::vector<joy_object*> set_vector;

struct joy_object {
  Type type;
  void* data;
  float float_data;
  std::string string_data;
  std::vector<joy_object*> list_data;
  set_vector set_data;
  voidFunction op;
  std::string ident;
  uint8_t list_len;

  joy_object(float f) : float_data(f), type(FLOAT) {} 
  joy_object(uint64_t i) : data((void*)i),type(INT)  {}
  joy_object(std::string s) : string_data(s) ,type(STR)  {}
  joy_object(std::string s, Type t) : string_data(s) ,type(t)  {}
  joy_object(std::vector<joy_object*> ob_list) : list_data(ob_list), type(LIST)  {}
  joy_object(Type t) : type(t)  {}
  joy_object(bool b) : data((void*)b), type(BOOL)  {} 
  joy_object(const joy_object& o) : type(o.type), data(o.data),
									float_data(o.float_data),
									string_data(o.string_data), list_data(o.list_data),
                                    set_data(o.set_data), op(o.op) {}
  joy_object() {}
};

uint64_t stack_ptr = STACK_SIZE;

// holds tagged pointers to joy objects
joy_object* stck[STACK_SIZE];

// k: string, v: pointer to joy object
std::unordered_map<std::string, joy_object*> heap;

// k:string, v: pointer to op (function)
//using op_ptr = *joy_object (void);
std::unordered_map<std::string, voidFunction> builtins;

Type get_type(joy_object* o) { return o->type; }

uint64_t get_int(joy_object* o) {
  return (uint64_t) o->data;
}

float get_float(joy_object* o) {
  return o->float_data;
}

std::string get_string(joy_object* o) {
  return o->string_data;
}

// N.B. chars are represented via strings
std::string get_char(joy_object* o) {
  return o->string_data;
}

bool get_bool(joy_object* o) {
  return (bool)o->data;
}

std::vector<joy_object*> get_list(joy_object* o) {
  return o->list_data;
}

std::vector<joy_object*> get_set(joy_object* o) {
  return o->list_data;
}

std::string get_op(joy_object* o) {
  return o->ident; 
}

std::variant<uint64_t, float, std::string, bool, std::vector<joy_object*>>
get_data(joy_object* o) {
  switch (o->type)
	{
	case INT:
	  return get_int(o);
	  break;
	case STR:
	  return get_string(o);
	  break;
	case CHAR:
	  return get_char(o);
	  break;
	case FLOAT:
	  return get_float(o);
	  break;
	case BOOL:
	  return get_bool(o);
	  break;
	case LIST:
	  return get_list(o);
	  break;
	case SET:
	  return get_set(o);
	  break;
	case OP:
	  return get_op(o);
	  break;
	defualt:
	  // TODO FIX THIS
	  return (uint64_t)0;
	} 
	return (uint64_t)0;
}

// checks head of stack for a list of terms/values
// then executes that list
void execute_term(bool is_x);

uint8_t check_stack(uint8_t x) {
  if (STACK_SIZE - stack_ptr >= x)
	return 1;
  else
	return 0;
}

uint8_t cur_stack_size() {
  return STACK_SIZE - stack_ptr;
}

/** VM MEMORY FUNCTIONS **/
// returns a handle for the stored object
joy_object* op_store(joy_object* obj, std::string name) {
  if (auto found = heap.find(name) != heap.end())
	return heap[name];

  heap[name] = obj;
  return heap[name];
}

joy_object* op_retrieve(std::string name) {
  uint64_t found;
  if (found = heap.find(name) == heap.end())
	return nullptr;

  return heap[name];
}

joy_object* op_get_head() {
  return stck[stack_ptr];
}

// returns a copy of the object in the stack at position pos
joy_object* op_copy(uint16_t pos) {
  if ((pos > stack_ptr) || (pos < stack_ptr)) {
	std::cout << "out of bounds copy!\n";
	return nullptr;
  }

  joy_object* res = new joy_object(stck[pos]);
  return res;
}

// gets the object from the stack at point pos
// without updating the stack pointer
joy_object* op_grab(uint64_t pos) {
  if ((pos > stack_ptr) || (pos < stack_ptr)) {
	std::cout << "out of bounds copy!\n";
	return nullptr;
  }

  return stck[pos]; 
}

joy_object* op_pop() {
  if (stack_ptr == STACK_SIZE) {
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
	
  }
  // printf("Stack ptr: %i\n", stack_ptr);
  return stck[stack_ptr++];
}

joy_object* op_peek(std::size_t offset) {
  if (stack_ptr == STACK_SIZE) {
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
	
  }
  return stck[stack_ptr + offset];
}

joy_object* op_push(joy_object* obj, Type type, joy_object* cur_stack=*stck) {
  if (--stack_ptr == 0)
	return nullptr; // mem error

  // printf("%i\n", (uint64_t)obj->data);
  obj->type = type;
  stck[stack_ptr] = obj;
  // printf("%i\n", stck[stack_ptr]);
  return obj;
}

joy_object* op_dup() {
  if (cur_stack_size() < 1){
	std::cout << "ERROR - nothing on stack!\n";
	return nullptr;
  }

  auto cur = new joy_object(*op_get_head());
  op_push(cur, cur->type);

  return cur;
}

joy_object* op_sub() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - stack size less than 2!\n";
	return nullptr;
  }

  //auto b = op_pop();
  //auto a = op_pop();

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  // possible valid combos:
  // int+int, float+int, float+float

  joy_object* c = new joy_object();

  // TODO: make switch here
  if (t1 == INT && t2 == INT) {
	auto int_a = get_int(a);
	auto int_b = get_int(b);

	auto res = int_a - int_b;
	c->data = (void*) res;
	op_pop();
	op_pop();
	op_push(c, INT);
  }

  if (t1 == FLOAT && t2 == INT) {
	auto flo_a = get_float(a);
	auto int_b = get_int(b);

	auto res = flo_a - float(int_b);
	c->float_data = res;
	op_push(c, FLOAT);
  }

  if (t1 == INT && t2 == FLOAT) {
	auto int_a = get_int(a);
	auto flo_b = get_float(b);

	auto res = float(int_a) - flo_b;
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
 
  }

  if (t1 == FLOAT && t2 == FLOAT) {
	auto flo_a = get_float(a);
	auto flo_b = get_float(b);

	auto res = flo_a - flo_b;
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
  }

  return c;
}

joy_object* op_add() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for addition!\n";
	return nullptr;
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  // possible valid combos:
  // int+int, float+int, float+float

  joy_object* c = new joy_object();

  if (t1 == INT && t2 == INT) {
	auto int_a = get_int(a);
	auto int_b = get_int(b);

	auto res = int_a + int_b;
	c->data = (void*) res;

	op_pop();
	op_pop();
	op_push(c, INT);
  }

  else if (t1 == FLOAT && t2 == INT) {
	auto flo_a = get_float(a);
	auto int_b = get_int(b);

	auto res = flo_a + float(int_b);
	c->float_data = res;

	op_pop();
	op_pop();
	op_push(c, FLOAT);
  }

  else if (t1 == INT && t2 == FLOAT) {
	auto int_a = get_int(a);
	auto flo_b = get_float(b);

	auto res = float(int_a) + flo_b;

	op_pop();
	op_pop();
	c->float_data = res;
	op_push(c, FLOAT);
 
  }

  else if (t1 == FLOAT && t2 == FLOAT) {
	auto flo_a = get_float(a);
	auto flo_b = get_float(b);
	auto res = flo_a + flo_b;

	c->float_data = res;

	op_pop();
	op_pop();
	op_push(c, FLOAT);
  }

  else if (t1 == STR && t2 == STR) {
	auto str_a = get_string(a);
	auto str_b = get_string(b);
	std::string res = "";
	res += str_a;
	res+= str_b;

	op_pop();
	op_pop();
	c->string_data = res;
	op_push(c, STR);
  }

  else {
	std::cout << "TYPE ERROR!\n";
  }

  return c;
}

joy_object* op_mul() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for multiplication!\n";
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  // possible valid combos:
  // int+int, float+int, float+float

  joy_object* c = new joy_object();

  if (t1 == INT && t2 == INT) {
	auto int_a = get_int(a);
	auto int_b = get_int(b);

	auto res = int_a * int_b;
	c->data = (void*) res;
	op_pop();
	op_pop();
	op_push(c, INT);
  }

  if (t1 == FLOAT && t2 == INT) {
	auto flo_a = get_float(a);
	auto int_b = get_int(b);

	auto res = flo_a * float(int_b);
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
  }

  if (t1 == INT && t2 == FLOAT) {
	auto int_a = get_int(a);
	auto flo_b = get_float(b);

	auto res = float(int_a) * flo_b;
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
 
  }

  if (t1 == FLOAT && t2 == FLOAT) {
	auto flo_a = get_float(a);
	auto flo_b = get_float(b);

	auto res = flo_a * flo_b;
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
  }

  return c;
}

joy_object* op_div() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for divison!\n";
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  // possible valid combos:
  // int+int, float+int, float+float

  joy_object* c = new joy_object();

  if (t1 == INT && t2 == INT) {
	auto int_a = get_int(a);
	auto int_b = get_int(b);

	auto res = int_a / int_b;
	c->data = (void*) res;
	op_pop();
	op_pop();
	op_push(c, INT);
  }

  if (t1 == FLOAT && t2 == INT) {
	auto flo_a = get_float(a);
	auto int_b = get_int(b);

	auto res = flo_a / float(int_b);
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
  }

  if (t1 == INT && t2 == FLOAT) {
	auto int_a = get_int(a);
	auto flo_b = get_float(b);

	auto res = float(int_a) / flo_b;
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
 
  }

  if (t1 == FLOAT && t2 == FLOAT) {
	auto flo_a = get_float(a);
	auto flo_b = get_float(b);

	auto res = flo_a / flo_b;
	c->float_data = res;
	op_pop();
	op_pop();
	op_push(c, FLOAT);
  }

  return c;
}

joy_object* op_lt() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for lessthan!\n";
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  if ((t1 != INT && t1 != FLOAT) || (t2 != INT && t2 != FLOAT)) {
	return nullptr;
  }

  joy_object* c = new joy_object(BOOL);

  if (get_data(b) > get_data(a))
	c->data = (void*)false;
  else
	c->data = (void*)true;

  op_pop();
  op_pop();
  op_push(c, c->type);
  return c;
}

joy_object* op_gt() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for greaterthan!\n";
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  if ((t1 != INT && t1 != FLOAT) || (t2 != INT && t2 != FLOAT))
	return nullptr;

  joy_object* c = new joy_object(BOOL);

  if (get_data(b) > get_data(a))
	c->data = (void*)true;
  else
	c->data = (void*)false;

  op_pop();
  op_pop();
  op_push(c, c->type);
  return c;
}

joy_object* op_cons() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - stack size less than 2!\n";
	return nullptr;
  }
  auto b = op_peek(0);
  auto a = op_peek(1);

  auto t1 = get_type(a);
  auto t2 = get_type(b);

  if (t1 != LIST or t2 != LIST) {
	std::cout << "ERROR - Needs 2 list!\n";
	return nullptr;
  }

  auto la = get_list(a);
  auto lb = get_list(b);


  std::vector<joy_object*> c;
  joy_object* head = new joy_object(LIST);
  c.push_back(head);
 for (int i = 1; i < lb.size(); i++)
	c.push_back(lb[i]);
  for (int i = 1; i < la.size(); i++)
	c.push_back(la[i]);
 
  auto c_obj = new joy_object(c);
  op_pop();
  op_pop();
  op_push(c_obj, LIST);
  return c_obj;
}

joy_object* op_swap() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - stack size less than 2!\n";
	return nullptr;
  }
  auto a = op_pop();
  auto b = op_pop();

  op_push(a, a->type);
  op_push(b, b->type);
  return a;
}

joy_object* op_rollup() {
  if (cur_stack_size() < 3) {
	std::cout << "ERROR - stack size less than 3!\n";
	return nullptr;
  }
  auto a = op_pop();
  auto b = op_pop();
  auto c = op_pop();

  op_push(c, c->type);
  op_push(a, a->type);
  op_push(b, b->type);

  return a;
}

joy_object* op_rolldown() {
  if (cur_stack_size() < 3) {
	std::cout << "ERROR - stack size less than 3!\n";
	return nullptr;
  }
  auto a = op_pop();
  auto b = op_pop();
  auto c = op_pop();

  op_push(b, b->type);
  op_push(c, c->type);
  op_push(a, a->type);

  return a;
}

joy_object* op_rotat() {
  if (cur_stack_size() < 3) {
	std::cout << "ERROR - stack size less than 3!\n";
	return nullptr;
  }
  auto a = op_pop();
  auto b = op_pop();
  auto c = op_pop();

  op_push(c, c->type);
  op_push(b, b->type);
  op_push(a, a->type);

  return a;
}

void execute_term(bool is_x = false) {
  joy_object* cur_list;
  if (is_x == false) 
	cur_list = op_pop();
  else
	cur_list = op_peek(0);

  if (cur_list->type != LIST)
	return;

  joy_object* temp_stack[STACK_SIZE/2];
  auto temp_stack_ptr = STACK_SIZE/2;

  auto l = get_list(cur_list);
  for(auto i = 1; i < l.size(); i++) {
	if (l[i]->type == OP){
	  l[i]->op();
	}
	else
	  //temp_stack[--temp_stack_ptr] = l[i];
	  op_push(l[i], l[i]->type);
  }

  //  for(auto i = temp_stack_ptr; i < STACK_SIZE/2; i++) {
  //op_push(temp_stack[i], temp_stack[i]->type);
  //}
}


// for maps/filters etc..
void execute_term_list(joy_object* l, joy_object* p) {
  if (cur_stack_size() < 2) {
	std::cout << "need at least 2 terms to execute\n";
	return;
  }
	
  auto cur_prog = op_pop();
  auto cur_list = op_pop();

  if (cur_list->type != LIST)
	return;

  joy_object* result_list = new joy_object(LIST);

  joy_object* temp_stack[STACK_SIZE/2];
  auto temp_stack_ptr = STACK_SIZE/2;


  auto li = get_list(cur_list);

  for(auto i = 1; i < li.size(); i++) {
	if (li[i]->type == OP){
	  li[i]->op();
	}
	else
	  //temp_stack[--temp_stack_ptr] = l[i];
	  op_push(li[i], li[i]->type);
  }

  //  for(auto i = temp_stack_ptr; i < STACK_SIZE/2; i++) {
  //op_push(temp_stack[i], temp_stack[i]->type);
  //}
}

void op_clear_stack() {
  while (stack_ptr < STACK_SIZE) {
	op_pop();
  }
}

void op_comb_i() {
  if (op_get_head()->type != LIST)
	std::cout << "i expects a LIST!\n";
  execute_term();
}

void op_comb_x() {
  if (op_get_head()->type != LIST)
	std::cout << "x expects a LIST!\n";
  execute_term(true);
}

void op_comb_dip() {
  if (op_get_head()->type != LIST)
	std::cout << "dip expects a LIST at the head position!\n";
  auto a = op_pop();
  auto b = op_pop();
  op_push(a, a->type);
  execute_term(false);
  op_push(b, b->type);
}

// e.g.
// [1 2 3] [dup +] map
// -> [2 4 3]
void op_comb_map() {
  if (cur_stack_size() < 2) {
	std::cout << "map expects 2 lists!\n";
	return;
  }

  // should be a single 'program' e.g. [dup +]
  auto b = op_peek(0); 
  // should be a list of values  e.g. [1 2 3]
  auto a = op_peek(1); 
  // puts [2 4 6] on the stack

  // verify that l_b is all ops
  // and that l_a are all values
  auto l_b = get_list(b);
  auto l_a = get_list(a);

  // testing if l_b is a program is slightly more difficult
  // than just testing if each item is an op
  // need to do arity checking for each item
  // mapping adds 1 value to the arity of the ops
  // e.g. [10 +] required one more value to have the
  // correct arity
  // [dup +] also requires one more value
  for(size_t i = 1; i < l_b.size(); i++) {
	if (l_b[i]->type != OP)
	  return; // TODO: Error
  }

  for(size_t i = 1; i < l_a.size(); i++) {
	if (l_a[i]->type == OP)
	  return; // TODO Error
  }
  
  // TODO:
  // mutliple results?
  std::vector<joy_object*> res_list;
  auto head = new joy_object(LIST);
  res_list.push_back(head);
  for(size_t i = 1; i < l_a.size(); i++) {
	op_push(l_a[i], l_a[i]->type);

	for (size_t j = 1; j < l_b.size(); j++) {
	  l_b[j]->op();
	}
	auto res = op_pop();
	res_list.push_back(res);
  }

  auto o = new joy_object(res_list);
  op_pop();
  op_pop();
	
  op_push(o, o->type);
  // TODO: can probably break this up into more smaller operators
  // but not sure atm
}

// e.g.
// [1 2 3] 0 [+] fold
// -> 6
void op_comb_fold() {
  if (cur_stack_size() < 3) {
	std::cout << "fold expects 3 objects!\n";
	return;
  }

  // e.g. ...
  // [+]
  auto c = op_peek(0); 
  // 0
  auto b = op_peek(1); 
  // [1 2 3]
  auto a = op_peek(2);  

  auto l_a = get_list(a);
  auto l_c = get_list(c);

  if (l_a.size() == 0 || l_c.size() == 0) {
	std::cout << "Expected first and third objects to be lists!\n";
	return;
  }

  // after type check...
  op_pop();
  op_pop();
  op_pop();

 
  op_push(b, b->type);
  for(size_t i = 1; i < l_a.size(); i++) {
	op_push(l_a[i], l_a[i]->type);
	for (size_t j = 1; j < l_c.size(); j++) {
	  l_c[j]->op();
	}
  }

  //auto o = new joy_object(res);
  // op_push(o, o->type);
}

/** SETUP BUILTINS **/
//  std::unordered_map<std::string, op_ptr> builtins;
void setup_builtins() {
  //using op_ptr = joy_object (*)();
  /** math ops**/
  builtins["+"] = (voidFunction)op_add;
  builtins["*"] = (voidFunction)op_mul;
  builtins["/"] = (voidFunction)op_div;
  builtins["-"] = (voidFunction)op_sub;
  builtins["<"] = (voidFunction)op_lt;
  builtins[">"] = (voidFunction)op_gt;
  //builtins["%"] = (voidFunction)op_mod;
  //builtins["or"] = (voidFunction)op_gt;
  //builtins["xor"] = (voidFunction)op_or;
  //builtins["and"] = (voidFunction)op_or;
  //builtins["not"] = (voidFunction)op_or;
  //builtins["neg"] = (voidFunction)op_or;
  //builtins["abs"] = (voidFunction)op_or;
  //builtins["acos"] = (voidFunction)op_or;
  //builtins["asin"] = (voidFunction)op_or;
  //builtins["atan"] = (voidFunction)op_or;
  //builtins["atan2"] = (voidFunction)op_or;
  //builtins["ceil"] = (voidFunction)op_or;
  //builtins["cos"] = (voidFunction)op_or;
  //builtins["cosh"] = (voidFunction)op_or;
  //builtins["exp"] = (voidFunction)op_or;
  //builtins["floor"] = (voidFunction)op_or;
  //builtins["frexp"] = (voidFunction)op_or;
  //builtins["ldexp"] = (voidFunction)op_or;
  //builtins["log"] = (voidFunction)op_or;
  //builtins["log10"] = (voidFunction)op_or;
  //builtins["modf"] = (voidFunction)op_or;
  //builtins["pow"] = (voidFunction)op_or;
  //builtins["sin"] = (voidFunction)op_or;
  //builtins["sqrt"] = (voidFunction)op_or;
  //builtins["sinh"] = (voidFunction)op_or;
  //builtins["tan"] = (voidFunction)op_or;
  //builtins["tanh"] = (voidFunction)op_or;
  //builtins["trunc"] = (voidFunction)op_or;
  //builtins["srand"] = (voidFunction)op_or;
  //builtins["pred"] = (voidFunction)op_or;
  //builtins["succ"] = (voidFunction)op_or;
  //builtins["max"] = (voidFunction)op_or;
  //builtins["min"] = (voidFunction)op_or;
  /** Files/Strings etc... **/


  /** stack ops**/
  builtins["clear"] = (voidFunction)op_clear_stack;
  builtins["pop"] = (voidFunction)op_pop;
  builtins["dup"] = (voidFunction)op_dup;
  builtins["cons"] = (voidFunction)op_cons;
  builtins["swap"] = (voidFunction)op_swap;
  builtins["rollup"] = (voidFunction)op_rollup;
  builtins["rolldown"] = (voidFunction)op_rolldown;
  builtins["rotate"] = (voidFunction)op_rolldown;

  //builtins["popd"] = (voidFunction)op_popd;
  //builtins["dupd"] = (voidFunction)op_dupd;
  //builtins["swapd"] = (voidFunction)op_swapd;
  //builtins["choice"] = (voidFunction)op_choice;

  /** combinators **/
  builtins["i"] = (voidFunction)op_comb_i;
  builtins["x"] = (voidFunction)op_comb_x;
  builtins["dip"] = (voidFunction)op_comb_dip;
  builtins["map"] = (voidFunction)op_comb_map;
  builtins["fold"] = (voidFunction)op_comb_fold;
}

/** PARSER **/
bool peek(std::string::const_iterator i,
				 std::string *input, std::string match) {
  if (input->compare(i - input->begin(), match.length(), match) == 0)
	return true;
  return false;
}

std::tuple<std::string::const_iterator, joy_object*>
parse_numeric(std::string::const_iterator i, std::string* input) {
  std::string cur_num;
  while(i != input->end()) {
	if (!std::isdigit(*i)) { // TODO: Floats
	  break;
	}
	cur_num += *i;  
	i++;
  }

  auto as_int = (uint64_t) stoi(cur_num);
  auto o = new joy_object(as_int);
  //op_push(o, INT);
  return {i,o};
}

// unused 
std::string::const_iterator
parse_op(std::string::const_iterator i, std::string* input) {
  std::string cur_op;

  while(i != input->end()) {
	if (std::isspace(*i)) {
	  break;
	}
	cur_op += *i;  
	i++;
  }

  joy_object* res;
  if (cur_op == "+") {
	res = op_add();
  }
  else if (cur_op == "/") {
	res = op_div();
  }
  else if (cur_op == "*") {
	res = op_mul();
  } 
  
  return i;
}

std::tuple<std::string::const_iterator, joy_object*>
parse_string(std::string::const_iterator i, std::string* input) {
  std::string cur_string;
  
  while(i != input->end()) {
	if (*i == '"') {
	  break;
	}
	cur_string += *i;  
	i++;
  }

  auto o = new joy_object(cur_string);
  //op_push(o, STR);
  return {i, o};
}

std::tuple<std::string::const_iterator, joy_object*>
parse_char(std::string::const_iterator i, std::string* input) {
  std::string cur_string;
  
  cur_string += *i;  
  i++;

  // ill-formed char
  if ((i != input->end()) && std::isalpha(*i)) {
	std::cout << "Chars must be single characters\n";
	return {i, nullptr};
  }

  auto o = new joy_object(cur_string);
  return {i, o};
}

bool is_builtin(std::string& cur) {
  if (auto found = builtins.find(cur) != builtins.end())
	return true;
  return false;
}

std::tuple<std::string::const_iterator, joy_object*>
parse_list(std::string::const_iterator it, std::string* input, Type t);
 
std::tuple<std::string::const_iterator, joy_object*>
parse_definition(std::string::const_iterator i, std::string* input) {
  joy_object* o;
  std::tie(i,o)  = parse_list(i, input, LIST);
  return {i,o};
}

std::tuple<std::string::const_iterator, joy_object*, bool, bool>
parse_ident(std::string::const_iterator i, std::string* input, bool exec=false) {
  std::string cur_ident;
  bool is_def = false;
  bool user_ident = false;

  while(i != input->end()) {
	if (*i == ' ' || *i == ']' || *i == '}') {
	  break;
	}
	cur_ident += *i;  
	i++;
  }

  joy_object* o = new joy_object(OP);
  if (is_builtin(cur_ident)){
	o->op = builtins[cur_ident];
	o->ident = cur_ident;
	if (exec)
	  builtins[cur_ident]();	
	//joy_object* cur_op = (joy_object*)op;
	//cur_op();
	return {i, o, is_def, user_ident};
  }

  // otherwise it points to a joy_object, which would be
  // a user defined operation or value 
  if (i != input->end()) {
	if (peek(++i, input, "==")) {
	  is_def = true;
	  ++i;
	  std::tie(i,o) = parse_definition(++i, input);
	  op_store(o, cur_ident);
	}
  }
  else {
	o = op_retrieve(cur_ident);
	if (o == nullptr)
	  std::cout << "Identifier " << cur_ident << " not found!\n";
	user_ident = true;
  }
  return {i, o, is_def, user_ident};
}

// returns a list of joy_objects that have been parsed
// for lists, sets, and definitions
joy_object* parse_line_to_obj(std::string input);

std::tuple<std::string::const_iterator, joy_object*>
parse_set(std::string::const_iterator i, std::string* input);
 
std::tuple<std::string::const_iterator, joy_object*>
parse_list(std::string::const_iterator it, std::string* input, Type t) {
  std::vector<joy_object*> cur_list;
  joy_object* head;
  bool is_def;
  bool user_ident;

  if (t == LIST)
	head = new joy_object(LIST);
  else
	head = new joy_object(SET);
	
  cur_list.push_back(head);

  while(it != input->end()){	
	joy_object* o = nullptr;
	if (*it == ']' || *it == '}') {
	  break;
	}
	if (std::isdigit(*it)) {
	  std::tie(it, o) = parse_numeric(it, input);
	  cur_list.push_back(o);
	}
	else if (*it == '"') {
	  std::tie(it, o) = parse_string(++it, input);
	  cur_list.push_back(o);
	  it++;
	}
	else if (*it == '['){
	  //it = std::get<0>(parse_list(++it, &input));
	  std::tie(it, o) = parse_list(++it, input, LIST);
	  std::cout << "PARSED_LIST" << std::endl;
	  cur_list.push_back(o);
	  // ++it; ?
	}
	else if (*it == '{') {
	  //it = std::get<0>(parse_set(++it, &input));
	  std::tie(it, o) = parse_set(++it, input);
	  cur_list.push_back(o);
	}
	else if (*it == 't') {
	  // true?
	  if (peek(it, input, "true")) {
		it+=4;
		o = new joy_object(true);
		cur_list.push_back(o);
	  }
	  else {
		std::tie(it, o, is_def, user_ident) = parse_ident(it, input);
		cur_list.push_back(o);
	  }
	}
	else if (*it == 'f') {
	  // false?
	  if (peek(it, input, "false")) {
		it+=5;
		o = new joy_object(false);
		cur_list.push_back(o);
	  }
	  else {
		std::tie(it, o, is_def, user_ident) = parse_ident(it, input);
		cur_list.push_back(o);
	  }
	}
	else if (*it == '+' || *it == '*' || *it == '/' || std::isalpha(*it)) {
	  std::tie(it, o, is_def, user_ident) = parse_ident(it, input); 
		cur_list.push_back(o);
	}
	else
	  ++it;
  }

  joy_object* o = new joy_object(cur_list);
  //op_push(o, LIST);
  return {it, o}; 
}

bool assert_set (std::vector<joy_object*> list) {
  auto cur_type = get_type(list[0]);
  for (auto l : list) {
	if (get_type(list[0]) != cur_type)
	  return false;
  }
  return true;
}

std::tuple<std::string::const_iterator, joy_object*>
parse_set(std::string::const_iterator i, std::string* input) {
  joy_object* cur_obj;
  std::tie(i, cur_obj) = parse_list(i, input, SET);
  auto cur_list = get_list(cur_obj);
  if (!(assert_set(cur_list))) {  
	  std::cout << "ERROR set must be homogeneous\n";
	  op_pop();
	}
  cur_list[0]->type = SET;
  return {i, cur_obj};
}

void print_data(std::variant<uint64_t,
				float, std::string, bool,
				std::vector<joy_object*>> data, joy_object* o, bool print_space = true) {
 	switch(data.index())
	  {
	  case 0:
		std::cout << std::get<uint64_t>(data);
		break;
	  case 1:
		std::cout << std::get<float>(data);
		break;
	  case 2:
		{
		  if (o->type == STR)
			std::cout << '"' << std::get<std::string>(data) << '"';
		  else if (o->type == CHAR)
			std::cout << "'" << std::get<std::string>(data);
		  else
			std::cout << std::get<std::string>(data);
		break;
		}
	  case 3:
		{
		auto res = std::get<bool>(data);
		if (res == false)
		  std::cout << "false";
		else
		  std::cout << "true";
		break;
		}
	  case 4:
		{
		  std::vector<joy_object*> obj = std::get<std::vector<joy_object*>>(data);	
		  if (get_type(obj[0]) == SET) {
			std::cout << "{ ";
			for (int i=1; i < obj.size(); i++)
			  if (i == obj.size()-1)
				print_data(get_data(obj[i]), obj[i], false);
			  else 
				print_data(get_data(obj[i]), obj[i]);
			std::cout << "} ";
		}
		else {
		  std::cout << "[";
		  for (int i=1; i < obj.size(); i++)
			if (i == obj.size()-1)
			  print_data(get_data(obj[i]), obj[i], false);
			else
			  print_data(get_data(obj[i]), obj[i]);
		  std::cout << "] ";
		}
		  return;
		}
	  default:
		break;
	  }
	if (print_space)
	  std::cout << " ";
}

void print_stack() {
  auto temp_ptr = STACK_SIZE-1;

  while (temp_ptr >= stack_ptr) {
	auto data = get_data(stck[temp_ptr]);
	if (temp_ptr == stack_ptr)
	  print_data(data, stck[temp_ptr], true);
	else
	  print_data(data, stck[temp_ptr]);
	temp_ptr--;
  }
}

void parse_line(std::string input, joy_object* cur_stack=*stck) {
  std::string::const_iterator it = input.begin();
  bool is_def = false;
  bool user_ident = false;
  while(it != input.end()){	
	joy_object* o = nullptr;
	if (std::isdigit(*it)) {
	  //it = std::get<std::string::const_iterator>(parse_numeric(it, &input));
	  std::tie(it, o) = parse_numeric(it, &input);
	  op_push(o, INT, cur_stack);
	}
	else if (*it == '"') {
	  //it = std::get<0>(parse_string(++it, &input));
	  std::tie(it, o) = parse_string(++it, &input);
	  op_push(o, STR, cur_stack);
	  it++;
	}
	else if (*it == '\'') {
	  std::tie(it, o) = parse_char(++it, &input);
	  if (o != nullptr)
		op_push(o, CHAR, cur_stack);
	  // throw away the line
	  else {
		it = input.end();
		continue;
	  }
	}
	//}
	else if (*it == '['){
	  //it = std::get<0>(parse_list(++it, &input));
	  std::tie(it, o) = parse_list(++it, &input, LIST);
	  op_push(o, LIST, cur_stack);
	  // ++it; ?
	}
	else if (*it == '{') {
	  //it = std::get<0>(parse_set(++it, &input));
	  std::tie(it, o) = parse_set(++it, &input);
	  op_push(o, SET, cur_stack);
	}
	else if (*it == 't') {
	  // true?
	  if (peek(it, &input, "true")) {
		it+=4;
		o = new joy_object(true);
		op_push(o, BOOL, cur_stack);
	  }
	  else { // TODO: same for other parse_ident operations?
		std::tie(it, o, is_def, user_ident) = parse_ident(it, &input, true);
	  }
	}
	else if (*it == 'f') {
	  // false?
	  if (peek(it, &input, "false")) {
		it+=5;
		o = new joy_object(false);
		op_push(o, BOOL, cur_stack);
	  }
	  else { // TODO: same for other parse_ident operations?
		std::tie(it, o, is_def, user_ident) = parse_ident(it, &input, true);
	  }
	}
	else if (*it == '+' || *it == '*' || *it == '/'
			 ||*it == '>' || *it == '<' || *it == '-' || std::isalpha(*it)) {
	  std::tie(it, o, is_def, user_ident) = parse_ident(it, &input, true); 
	  // if that was not a defintion, push it to the stack
	  if (o == nullptr) {
		it = input.end();
		continue;
	  }
	  if (user_ident) {
		op_push(o, o->type, cur_stack);
		op_comb_i(); // if it's a user def, it's stored as a list
	  }
	}
	else
	  ++it;
  }
  //print_stack();
}

void run_interpreter() {
  std::string input_line;

  while(true) {
	std::cout << "joy~> ";
	print_stack();
	std::getline(std::cin, input_line);
	if(input_line == "quit")
	  break;
	else
	  parse_line(input_line);

	std::cout << "\n";
  }
  
  return;
}

int main() {
  printf("RUNNING JOYVM\n");
  setup_builtins();
  run_interpreter();
}
