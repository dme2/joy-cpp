#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

/*
 * TODO:
 *  [x] setup builtin op dictionary
 *  [x] finish definition parsing
 *  [x] test sets
 *  [x] float parsing
 *  [] error checking on parsing 
 *  [?] fix list parsing
 *  [x] char parsing
 *  [] some ops and combinaors...
 *     [x] and
 *     [x] or
 *     [x] not
 *     [x] reverse
 *     [x] dip
 *     [x] first
 *     [x] rest
 *     [x] at
 *     [x] ifte
 *     [x] step
 *     [x] filter
 *     [x] fold
 *     [x] cleave
 *     [x] split
 *     [x] uncons
 *     [x] small 
 *     [x] pred
 *     [x] succ
 *     [] treerec
 *     [x] size
 *     [x] swons
 *     [x] x
 *     [] primrec
 *     [] powerlist
 *     [] linrec
 *     [] binrec
 *     ... and many more
 *  [x] fix type checking (should happen before popping the values off somehow?)
 *  [] add block (DEFINITION, LIBRA) parsing
 *  [] add file/stdin parsing modes
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
  std::vector<joy_object*> list_data{};
  set_vector set_data;
  voidFunction op;
  std::string ident;
  uint8_t list_len;

  joy_object(float f) : float_data(f), type(FLOAT) {list_data.resize(0);} 
  joy_object(int64_t i) : data((void*)i),type(INT)  {}
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

int64_t get_int(joy_object* o) {
  return (int64_t) o->data;
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

std::variant<int64_t, float, std::string, bool, std::vector<joy_object*>>
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
	  return (int64_t)0;
	} 
	return (int64_t)0;
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
  int64_t found;
  if ((found = heap.find(name) == heap.end()))
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

joy_object* op_first() {
  if (cur_stack_size() < 1){
	std::cout << "ERROR - nothing on stack!\n";
	return nullptr;
  }

  auto a = op_peek(0);
  if (a->type != STR && a->type != LIST) {
	std::cout << "ERROR - first requires a string or a list!\n";
	return nullptr;
  }

  if (a->type == STR) {
	auto str = a->string_data;
	auto res = str.substr(0,1);
	auto res_o = new joy_object(res);
	res_o->type = CHAR;
	op_pop();
	op_push(res_o, res_o->type);
	return res_o;
  }
  else{
	auto l_a = get_list(a);
	auto res = new joy_object(l_a[1]);
	res->type = l_a[1]->type;
	op_pop();
	op_push(res, res->type);
	return res;
  }
}

joy_object* op_uncons() {
  if (cur_stack_size() < 1){
	std::cout << "ERROR - nothing on stack!\n";
	return nullptr;
  }

  auto a = op_peek(0);
  if (a->type != STR && a->type != LIST) {
	std::cout << "ERROR - first requires a string or a list!\n";
	return nullptr;
  }

  if (a->type == STR) {
	auto str = a->string_data;
	auto res = str.substr(0,1);
	auto res2 = str.substr(1, str.length());
	auto res_o = new joy_object(res);
	auto res_o2 = new joy_object(res2);
	res_o->type = CHAR;
	op_pop();
	op_push(res_o, res_o->type);
	op_push(res_o2, res_o2->type);
	return res_o;
  }
  else{
	auto l_a = get_list(a);
	auto res = new joy_object(l_a[1]);
	auto res2 = new joy_object(std::vector<joy_object*>(l_a.begin()+1, l_a.end()));
	res->type = l_a[1]->type;
	op_pop();
	op_push(res, res->type);
	op_push(res2, LIST);
	return res;
  }
}

joy_object* op_at() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for at!\n";
	return nullptr;
  }

  auto a = op_peek(0);
  auto b = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);


  joy_object* c;
  if (t1 == INT && t2 == LIST) {
	auto int_a = get_int(a);
	auto l_b = get_list(b);

	if (int_a >= l_b.size()-1) {
	  std::cout << "index out of bounds!\n";
	  return nullptr;
	}
	auto res = l_b[int_a+1];
	op_pop();
	op_pop();
	op_push(res, res->type);
  }
  else {
	std::cout << "type error - at requires an index and a list" ;
	return nullptr;
  }

  return c;
}

joy_object* op_eq() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for at!\n";
	return nullptr;
  }

  auto a = op_peek(0);
  auto b = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);


  // TODO: sets
  joy_object* c;
  if (t1 == INT && t2 == INT) {
	auto int_a = get_int(a);
	auto int_b = get_int(b);

	auto res = int_a == int_b;
	auto res_o = new joy_object(res);
	op_pop();
	op_pop();
	op_push(res_o, BOOL);
  }
  else {
	std::cout << "type error - at requires an index and a list" ;
	return nullptr;
  }

  return c;
}

joy_object* op_rest() {
  if (cur_stack_size() < 1) {
	std::cout << "ERROR - nothing on stack!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  if (a->type != STR && a->type != LIST) {
	std::cout << "ERROR - first requires a string or a list!\n";
	return nullptr;
  }

  if (a->type == STR) {
	auto str = a->string_data;
	auto res = str.substr(1,str.length());
	auto res_o = new joy_object(res);
	res_o->type = STR;
	op_pop();
	op_push(res_o, res_o->type);
	return res_o;
  }
  else{
	auto l_a = get_list(a);
	std::vector<joy_object*> res_l;
	auto head = new joy_object(LIST);
	res_l.push_back(head);
	for (int i = 2; i < l_a.size(); i++) {
	  res_l.push_back(l_a[i]);
	}
	auto res = new joy_object(res_l);
	op_pop();
	op_push(res, res->type);
	return res;
  }
}

joy_object* op_reverse() {
  if (cur_stack_size() < 1) {
	std::cout << "ERROR - empty stack!\n";
	return nullptr;
  }

    auto a = op_peek(0);

  if (a->type != STR && a->type != LIST) {
	std::cout << "ERROR - first requires a string or a list!\n";
	return nullptr;
  }

  if (a->type == STR) {
	auto str = a->string_data;
	std::reverse(str.begin(), str.end());
	auto res_o = new joy_object(str);
	op_pop();
	op_push(res_o, res_o->type);
	return res_o;
  }
  else{
	auto l_a = get_list(a);
	std::vector<joy_object*> res_l;
	auto head = new joy_object(LIST);
	res_l.push_back(head);
	for (int i = l_a.size()-1; i > 0; i--){
	  res_l.push_back(l_a[i]);
	}
	auto res = new joy_object(res_l);
	op_pop();
	op_push(res, res->type);
	return res;
  }

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
	return nullptr;
  }

  return c;
}

joy_object* op_pred() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) { // and float?
	auto int_a = get_int(a);
	auto res = int_a-1;
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else {
	std::cout << "pred requires numeric valuess!";
	return nullptr;
  }
  return c;
}

joy_object* op_succ() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) { // and float?
	auto int_a = get_int(a);
	auto res = int_a+1;
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else {
	std::cout << "succ requires numeric valuess!";
	return nullptr;
  }
  return c;
}


joy_object* op_max() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(1);
  auto b = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  joy_object* c;
  if (t1 == INT && t2 == INT) { // and float?
	auto int_a = get_int(a);
	auto int_b = get_int(b);
	auto res = std::max(int_a, int_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT && t2 == FLOAT) {
	auto float_a = get_float(a);
	auto float_b = get_float(b);
	auto res = std::max(float_a, float_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "max requires numeric valuess!";
	return nullptr;
  }
  return c;
}

joy_object* op_min() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(1);
  auto b = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  joy_object* c;
  if (t1 == INT && t2 == INT) { // and float?
	auto int_a = get_int(a);
	auto int_b = get_int(b);
	auto res = std::min(int_a, int_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT && t2 == FLOAT) {
	auto float_a = get_float(a);
	auto float_b = get_float(b);
	auto res = std::min(float_a, float_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "pred requires numeric valuess!";
	return nullptr;
  }
  return c;
}

joy_object* op_maxint() {
  auto maxint = new joy_object((int64_t)INT_MAX);
  op_push(maxint, INT);
  return maxint;
}

joy_object* op_neg() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	auto int_a = get_int(a);
	auto res = -int_a;
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = -float_a;
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "neg requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_abs() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	auto int_a = get_int(a);
	auto res = std::abs(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::abs(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "abs requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_acos() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::acos(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::acos(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "acos requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_asin() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::asin(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::asin(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "asin requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_atan() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::atan(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::atan(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "atan requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_atan2() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(1);
  auto b = op_peek(0);
  

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  joy_object* c;
  if (t1 == INT && t2 == INT) {
	int int_a = get_int(a);
	int int_b = get_int(b);
	float res = std::atan2(int_a, int_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT && t2 == FLOAT) {
	auto float_a = get_float(a);
	auto float_b = get_float(b);
	auto res = std::atan2(float_a, float_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "atan2 requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_ceil() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::ceil(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::ceil(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "ceil requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_cos() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::cos(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::cos(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "cos requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_cosh() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::cosh(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::cosh(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "cosh requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_exp() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::exp(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::exp(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "exp requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_floor() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::floor(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::floor(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "floor requires numeric values!";
	return nullptr;
  }
  return c;
}


joy_object* op_ldexp() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(1);
  auto b = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  joy_object* c;
  if (t1 == INT && t2 == INT) {
	int int_a = get_int(a);
	int int_b = get_int(b);
	float res = std::ldexp(int_a, int_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT && t1 == FLOAT) {
	auto float_a = get_float(a);
	auto float_b = get_float(b);
	auto res = std::ldexp(float_a, float_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "ldexp requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_log() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::log(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::log(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "log requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_log10() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::log10(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::log10(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "log10 requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_pow() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(1);
  auto b = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  joy_object* c;
  if (t1 == INT && t2 == INT) {
	int int_a = get_int(a);
	int int_b = get_int(b);
	int64_t res = std::pow(int_a, int_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto float_b = get_float(b);
	auto res = std::pow(float_a, float_b);
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "pow requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_sin() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::sin(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::sin(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "sin requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_sqrt() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::sqrt(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::sqrt(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "sqrt requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_sinh() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::sinh(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::sinh(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "sinh requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_tan() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::tan(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::tan(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "tan requires numeric values!";
	return nullptr;
  }
  return c;
}


joy_object* op_tanh() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::tanh(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::tanh(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "tanh requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_trunc() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	float res = std::trunc(int_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, INT);
  }
  else if (t1 == FLOAT) {
	auto float_a = get_float(a);
	auto res = std::trunc(float_a);
	c = new joy_object(res);

	op_pop();
	op_push(c, FLOAT);

  }
  else {
	std::cout << "trunc requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_srand() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == INT) {
	int int_a = get_int(a);
	std::srand(int_a);
	//c = new joy_object(res);

	op_pop();
	//op_push(c, INT);
  }
  else {
	std::cout << "trunc requires numeric values!";
	return nullptr;
  }
  return c;
}

joy_object* op_rand() {
  //if (STACK_SIZE - stack_ptr < 1){
  //std::cout << "ERROR - stack empty!\n";
  //return nullptr;
  //}

  //auto a = op_peek(0);

  // TODO: type checking
  //Type t1 = get_type(a);

  joy_object* c;
  std::rand();
  return c;
}

joy_object* op_not () {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }

  auto a = op_peek(0);

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c;
  if (t1 == BOOL) {
	auto bool_a = get_bool(a);
	auto res = !bool_a;
	c = new joy_object(res);

	op_pop();
	op_push(c, BOOL);
  }
  else {
	std::cout << "not requires boolean valuess!";
	return nullptr;
  }
  return c;
}

joy_object* op_and() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for and!\n";
	return nullptr;
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);


  joy_object* c;
  if (t1 == BOOL && t2 == BOOL) {
	auto int_a = get_bool(a);
	auto int_b = get_bool(b);

	auto res = int_a && int_b;
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, BOOL);
  }
  else {
	std::cout << "or requires boolean valuess or sets!";
	return nullptr;
  }

  return c;
}

joy_object* op_or() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for or!\n";
	return nullptr;
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);


  joy_object* c;
  if (t1 == BOOL && t2 == BOOL) {
	auto int_a = get_bool(a);
	auto int_b = get_bool(b);

	auto res = int_a || int_b;
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, BOOL);
  }
  else {
	std::cout << "or requires bools or sets!";
	return nullptr;
  }

  return c;
}

joy_object* op_xor() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for xor!\n";
	return nullptr;
  }

  auto b = op_peek(0);
  auto a = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  joy_object* c;
  if (t1 == BOOL && t2 == BOOL) {
	auto int_a = get_bool(a);
	auto int_b = get_bool(b);

	auto res = int_a != int_b;
	c = new joy_object(res);

	op_pop();
	op_pop();
	op_push(c, BOOL);
  }
  else {
	std::cout << "xor requires bools or sets!";
	return nullptr;
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

joy_object* op_rem() {
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

	auto res = int_a % int_b;
	c->data = (void*) res;
	op_pop();
	op_pop();
	op_push(c, INT);
	return c;
  }
  else
	return nullptr;
}

joy_object* op_null() {
  if (STACK_SIZE - stack_ptr < 1){
	std::cout << "Error - empty stack\n";
	return nullptr;
  }

  auto a = op_pop();

  // TODO: type checking
  Type t1 = get_type(a);

  joy_object* c; 
  switch(t1)
	  {
	  case INT:
		{
		  auto i = get_int(a);
		  if (i == 0) {
			c = new joy_object(true);
		  }
		  else
			c = new joy_object(false);
		  break;
		}
	  case FLOAT:
		{
		  auto f = get_float(a);
		  if (f == 0.0)
			c = new joy_object(true);
		  else
			c = new joy_object(false);
		  break;
		}
	  case STR:
		{
		  auto s = get_string(a);
		  if (s == "")
			c = new joy_object(true);
		  else
			c = new joy_object(false);
		  break;
		}
	  case LIST:
		{
		  auto l = get_list(a);
		  if (l.size() == 1)
			c  = new joy_object(true);
		  else
			c = new joy_object(false);
		  break;
		}
	  case SET:
		{
		  auto s = get_set(a);
		  if (s.size() == 1)
			c = new joy_object(true);
		  else
			c = new joy_object(false);
		  break;
		}
	  case BOOL:
		{
		  auto b = get_bool(a);
		  if (!b)
			c = new joy_object(true);
		  else
			c = new joy_object(false);
		  break;
		}
	  default:
		{
		  std::cout << "UKNOWN ERROR!\n";
		  return nullptr;
		}
	  }
  
  op_push(c, c->type);
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

  auto a = op_peek(0);
  auto b = op_peek(1);

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

joy_object* op_gte() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for greaterthan!\n";
  }

  auto a = op_peek(0);
  auto b = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  if ((t1 != INT && t1 != FLOAT) || (t2 != INT && t2 != FLOAT))
	return nullptr;

  joy_object* c = new joy_object(BOOL);

  if (get_data(b) >= get_data(a))
	c->data = (void*)true;
  else
	c->data = (void*)false;

  op_pop();
  op_pop();
  op_push(c, c->type);
  return c;
}

joy_object* op_lte() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for greaterthan!\n";
  }

  auto a = op_peek(0);
  auto b = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  if ((t1 != INT && t1 != FLOAT) || (t2 != INT && t2 != FLOAT))
	return nullptr;

  joy_object* c = new joy_object(BOOL);

  if (get_data(b) <= get_data(a))
	c->data = (void*)true;
  else
	c->data = (void*)false;

  op_pop();
  op_pop();
  op_push(c, c->type);
  return c;
}


joy_object* op_ne() {
  if (STACK_SIZE - stack_ptr < 2){
	std::cout << "ERROR - need at least two operands for greaterthan!\n";
	return nullptr;
  }

  auto a = op_peek(0);
  auto b = op_peek(1);

  // TODO: type checking
  Type t1 = get_type(a);
  Type t2 = get_type(b);

  joy_object* c = new joy_object(BOOL);

  if ((t1 == INT && t2 == INT) ||
	  (t1 == FLOAT && t2 == FLOAT) ||
	  (t1 == STR && t2 == STR) ||
	  (t1 == CHAR && t2 == CHAR)||
	  (t1 == BOOL && t2 == BOOL)) {
	  

	if (get_data(b) != get_data(a))
	  c->data = (void*)true;
	else
	  c->data = (void*)false;

	op_pop();
	op_pop();
	op_push(c, c->type);
	return c;
  }
  else
	return nullptr;
}

// TODO:
joy_object* op_cons() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - stack size less than 2!\n";
	return nullptr;
  }
  auto b = op_peek(0);
  auto a = op_peek(1);

  auto t1 = get_type(a);
  auto t2 = get_type(b);

  // TODO: sets
  if (t2 != LIST) {
	std::cout << "ERROR - second param should be a list!\n";
	return nullptr;
  }

  auto lb = get_list(b);


  std::vector<joy_object*> c;
  joy_object* head = new joy_object(LIST);
  c.push_back(head);
  c.push_back(a);
  for (int i = 1; i < lb.size(); i++)
	c.push_back(lb[i]);
 
  auto c_obj = new joy_object(c);
  op_pop();
  op_pop();
  op_push(c_obj, LIST);
  return c_obj;
}

joy_object* op_size(bool exec = true) {
  if (cur_stack_size() < 1) {
	std::cout << "ERROR on size - stack size less than 1!\n";
	return nullptr;
  }
  auto a = op_peek(0);
  auto l_a = get_list(a);
  auto sz = (int64_t)l_a.size();
  auto res = sz == 0 ? new joy_object(sz) : new joy_object(sz-1);
  if (exec) {
	op_pop();
	op_push(res, INT);
  }
  return res;
}

joy_object* op_small() {
  if (cur_stack_size() < 1) {
	std::cout << "ERROR on size - stack size less than 1!\n";
	return nullptr;
  }
  auto res1 = op_size(false);
  auto i_res = get_int(res1);
  auto res = i_res <= 1 ? new joy_object(true) : new joy_object(false);
  op_pop();
  op_push(res, BOOL);
  return res;
}

joy_object* op_swap() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR on swap - stack size less than 2!\n";
	return nullptr;
  }
  auto a = op_pop();
  auto b = op_pop();

  op_push(a, a->type);
  op_push(b, b->type);
  return a;
}

joy_object* op_of() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - need at least two operands for of\n";
	return nullptr;
  }

  op_swap();
  return(op_at());
}

joy_object* op_swons() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - stack size less than 2\n";
	return nullptr;
  }
  op_swap();
  return (op_cons());
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

joy_object* op_rotate() {
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

  auto l = get_list(cur_list);
  for(auto i = 1; i < l.size(); i++) {
	if (l[i]->type == OP){
	  l[i]->op();
	}
	else {
	  //temp_stack[--temp_stack_ptr] = l[i];
	  op_push(l[i], l[i]->type);
	}
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

joy_object* op_id () {
  return nullptr;
}

void op_clear_stack () {
  while (stack_ptr < STACK_SIZE) {
	op_pop();
  }
}

joy_object* op_unstack() {
  if (cur_stack_size() < 1 || op_get_head()->type != LIST) {
	std::cout << "unstack expects a list!\n";
	return nullptr;
  }
  auto a = op_pop();
  op_clear_stack();

  auto l_a = get_list(a);

  for (int i = l_a.size()-1; i > 0; i--) {
	op_push(l_a[i], l_a[i]->type);
  }

  return nullptr;
  
}

joy_object* op_ord() {
  if (cur_stack_size() < 1 || op_get_head()->type != CHAR) {
	std::cout << "unstack expects a char!\n";
	return nullptr;
  }
  auto a = op_pop();

  auto c =  get_char(a);
  auto res = (int) c[0];
  auto res_o = new joy_object((int64_t) res);
  op_push(res_o, INT);
  
  return res_o;
}

joy_object* op_chr() {
  if (cur_stack_size() < 1 || op_get_head()->type != INT) {
	std::cout << "unstack expects a char!\n";
	return nullptr;
  }
  auto a = op_pop();

  auto c =  get_int(a);
  if (c < 0 || c > 127) {
	std::cout << "invalid int for conversion to char\n";
	return nullptr;
  }
  char res = '0' + c;
  auto res_str = "";
  res_str+=res;
  auto res_o = new joy_object(res_str, CHAR);

  op_push(res_o, CHAR);
  
  return res_o;
}

joy_object* op_stack() {
  if (cur_stack_size() < 1) {
	std::cout << "ERROR - stack empty!\n";
	return nullptr;
  }
  auto len = cur_stack_size();
  std::vector<joy_object*> res_list;
  auto head = new joy_object(LIST);
  res_list.push_back(head);
 
  for (int i = 0; i < len; i++) {
	auto x = op_peek(i);
	res_list.push_back(x);
  }

  auto res_o = new joy_object(res_list);
  op_push(res_o, LIST);
  
  return res_o;
}


void op_comb_i () {
  if (op_get_head()->type != LIST)
	std::cout << "i expects a LIST!\n";
  execute_term();
}

void op_comb_split () {
  if (cur_stack_size() < 2) {
	std::cout << "Error - split expects 2 params\n!";
	return;
  }

  // pivot
  auto a = op_peek(0);
  // list
  auto b = op_peek(1);

  if (a->type != INT || b->type != LIST) {
	std::cout << "type error on params!\n";
	return;
  }

  std::vector<joy_object*> res_list1;
  std::vector<joy_object*> res_list2;
  auto head1 = new joy_object(LIST);
  auto head2 = new joy_object(LIST);
  res_list1.push_back(head1);
  res_list2.push_back(head2);

  auto pivot = get_int(a);
  auto l_b = get_list(b);

  for (int i = 1; i < l_b.size(); i++) {
	if (i <= pivot) {
	  res_list1.push_back(l_b[i]);
	}
	else
	  res_list2.push_back(l_b[i]);
  } 

  op_pop();
  op_pop();
  op_push(new joy_object(res_list1), LIST);
  op_push(new joy_object(res_list2), LIST);
  return;

}

// e.g.
// 2 [2 3 +] app1
// -> 5
//
// 2 [1 +] app1
// -> 3 (I think?)
//
// 2 3 [1 2 3] app1
// -> 2 1 2 3
void op_comb_app1() {
  if (cur_stack_size() < 2) {
	std::cout << "Error - app1 expects 2 params\n!";
	return;
  }

  // list 
  auto a = op_peek(0);
  // X
  auto b = op_peek(1);
  int b_pos = stack_ptr + 1;

  if (a->type != LIST) {
	std::cout << "type error on params!\n";
	return;
  }

  op_comb_i();

  if (stack_ptr == b_pos) // b was used in the execution
	return;
  else {
	stck[b_pos] = nullptr;
  }

  return;
}

// e.g.
// 1 2 [1 +] -> 3
// 1 2 [1 2 +] -> 3
void op_comb_app11() {
  if (cur_stack_size() < 3) {
	std::cout << "Error - app11 expects 3 params\n!";
	return;
  }

  // list 
  auto a = op_peek(0);
  // Y
  auto b = op_peek(1);
  // X
  auto c = op_peek(2);

  int b_pos = stack_ptr + 1;
  int c_pos = stack_ptr + 2;

  if (a->type != LIST) {
	std::cout << "type error on params!\n";
	return;
  }

  op_comb_i();
  
  // TODO: fix this...
  if (stack_ptr == b_pos) {
	stck[c_pos] = nullptr;
  }
  else if (stack_ptr == c_pos) {
	return;
  }
  else {
	stck[b_pos] = nullptr;
	stck[c_pos] = nullptr;
  }
  return;
}

// e.g.
// X Y1 Y2 [P1] app12 -> R1 R2
// Y1 produces R1 and R2 produces R2
// X, Y1, Y2 are then dropped from the stack
void op_comb_app12() {
  if (cur_stack_size() < 4) {
	std::cout << "Error - app12 expects 4 params\n!";
	return;
  }

  // list 
  auto a = op_peek(0);
  // Y1 
  auto b = op_peek(1);
  // Y2 
  auto c = op_peek(2);
  // X 
  auto d = op_peek(3);

  int b_pos = stack_ptr + 1;
  int c_pos = stack_ptr + 2;
  int d_pos = stack_ptr + 3;

  if (a->type != LIST) {
	std::cout << "type error on params!\n";
	return;
  }

  // POP everything;
  auto p1 = op_pop();
  auto y2 = op_pop();
  auto y1 = op_pop();
  auto  x = op_pop();

  // TODO:
  // handle x?
  op_push(y1, y1->type);
  op_push(p1, p1->type);
  op_comb_i();

  op_push(y2, y2->type);
  op_push(p1, p1->type);
  op_comb_i();

  return;
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

joy_object* op_popd() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - popd requires 2 params!\n";
	return nullptr;
  }
  std::vector<joy_object*> l1;
  auto head = new joy_object(LIST);
  l1.push_back(head);
	
  auto op_obj = new joy_object(OP);
  op_obj->op = (voidFunction)op_pop;
  l1.push_back(op_obj);
  auto obj = new joy_object(l1);
  op_push(obj, LIST);
  op_comb_dip();
  return nullptr;
}

joy_object* op_dupd() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - dupd requires 2 params!\n";
	return nullptr;
  }
  std::vector<joy_object*> l1;
  auto head = new joy_object(LIST);
  l1.push_back(head);
	
  auto op_obj = new joy_object(OP);
  op_obj->op = (voidFunction)op_dup;
  l1.push_back(op_obj);
  auto obj = new joy_object(l1);
  op_push(obj, LIST);
  op_comb_dip();
  return nullptr;
}

joy_object* op_swapd() {
  if (cur_stack_size() < 3) {
	std::cout << "ERROR - error swapd requires 3 params!\n";
	return nullptr;
  }
  std::vector<joy_object*> l1;
  auto head = new joy_object(LIST);
  l1.push_back(head);
	
  auto op_obj = new joy_object(OP);
  op_obj->op = (voidFunction)op_swapd;
  l1.push_back(op_obj);
  auto obj = new joy_object(l1);
  op_push(obj, LIST);
  op_comb_dip();
  return nullptr;
}

joy_object* op_rollupd() {
  if (cur_stack_size() < 4) {
	std::cout << "ERROR - rollupd requires 4 params!\n";
	return nullptr;
  }
  std::vector<joy_object*> l1;
  auto head = new joy_object(LIST);
  l1.push_back(head);
	
  auto op_obj = new joy_object(OP);
  op_obj->op = (voidFunction)op_rollupd;
  l1.push_back(op_obj);
  auto obj = new joy_object(l1);
  op_push(obj, LIST);
  op_comb_dip();
  return nullptr;
}

joy_object* op_rolldownd() {
  if (cur_stack_size() < 4) {
	std::cout << "ERROR - rolldown requires 4 params!\n";
	return nullptr;
  }
  std::vector<joy_object*> l1;
  auto head = new joy_object(LIST);
  l1.push_back(head);
	
  auto op_obj = new joy_object(OP);
  op_obj->op = (voidFunction)op_rolldownd;
  l1.push_back(op_obj);
  auto obj = new joy_object(l1);
  op_push(obj, LIST);
  op_comb_dip();
  return nullptr;
}

joy_object* op_rotated() {
  if (cur_stack_size() < 4) {
	std::cout << "ERROR - rotated requires 4 params!\n";
	return nullptr;
  }
  std::vector<joy_object*> l1;
  auto head = new joy_object(LIST);
  l1.push_back(head);
	
  auto op_obj = new joy_object(OP);
  op_obj->op = (voidFunction)op_rotated;
  l1.push_back(op_obj);
  auto obj = new joy_object(l1);
  op_push(obj, LIST);
  op_comb_dip();
  return nullptr;
}

joy_object* op_choice() {
  if (cur_stack_size() < 3) {
	std::cout << "ERROR - choice requires 3 params\n";
	return nullptr;
  }

  auto t = op_peek(2);
  if (t->type != BOOL) {
	std::cout << "ERROR - need bool type!\n";
	return nullptr;
  }

  auto a = op_pop();
  auto b = op_pop();

  t = op_pop();
  if (get_bool(t) == true) {
	op_push(b, b->type);
  }
  else
	op_push(a, a->type);
  return nullptr;
}

joy_object* op_compare() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - compare requires 2 params\n";
	return nullptr;
  }

  auto a = op_pop();
  auto b = op_pop();
  auto t_a = a->type;
  auto t_b = b->type;

  auto neg1 = new joy_object((int64_t)-1);
  auto zero = new joy_object((int64_t)0);
  auto pos1 = new joy_object((int64_t)1);

  if ((t_a == INT && t_b == INT) || (t_a == FLOAT && t_b == FLOAT)) {
	auto x = t_a == INT ? get_int(a) : get_float(a);
	auto y = t_b == INT ? get_int(b) : get_float(b);
	if (y <= x) {
	  op_push(neg1, INT);
	  return neg1;
	}
	else if (y == x) {
	  op_push(zero, INT);
	  return zero;
	}
	else if (y >= x) {
	  op_push(pos1, INT);
	  return pos1;
	}
  }
  else {
	std::cout << "type error!\n";
  }

  return nullptr;
}


joy_object* op_opcase() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - opcase requires 2 params\n";
	return nullptr;
  }

  auto a = op_peek(0);
  auto b = op_peek(1);
  auto t_a = a->type;
  auto t_b = b->type;

  if (t_a != LIST) {
	std::cout << "opcase expects a list param!\n";
  }

  std::vector<joy_object*> res_list;
  auto head = new joy_object(LIST);
  res_list.push_back(head);
	
  auto l_a = get_list(a);
  for (int i = 0; i < l_a.size(); i++) {
	if (l_a[i]->type != LIST) {
	  std::cout << "opcase requires a list of lists!\n";
	  return nullptr;
	}
	if (get_list(l_a[i]).size() > 2) {
	  auto temp_list = get_list(l_a[i]);
	  if (temp_list[1]->type == t_b) {
		for (int j = 2; j < temp_list.size(); j++) {
		  res_list.push_back(temp_list[j]);
		}
		op_pop();
		op_pop();
		auto res_obj = new joy_object(res_list);
		op_push(res_obj, LIST);
		return res_obj;
	  }
	}
  }

  return nullptr;
}

joy_object* op_case() {
  if (cur_stack_size() < 2) {
	std::cout << "ERROR - opcase requires 2 params\n";
	return nullptr;
  }

  auto a = op_peek(0);
  auto b = op_peek(1);
  auto t_a = a->type;
  auto t_b = b->type;

  if (t_a != LIST) {
	std::cout << "opcase expects a list param!\n";
  }

  std::vector<joy_object*> res_list;
  auto head = new joy_object(LIST);
  res_list.push_back(head);
	
  auto l_a = get_list(a);
  for (int i = 0; i < l_a.size(); i++) {
	if (l_a[i]->type != LIST) {
	  std::cout << "opcase requires a list of lists!\n";
	  return nullptr;
	}
	if (get_list(l_a[i]).size() > 2) {
	  auto temp_list = get_list(l_a[i]);
	  if (get_data(temp_list[1]) == get_data(b)) {
		for (int j = 2; j < temp_list.size(); j++) {
		  res_list.push_back(temp_list[j]);
		}
		op_pop();
		op_pop();
		auto res_obj = new joy_object(res_list);
		op_push(res_obj, LIST);
		op_comb_i();
		return res_obj;
	  }
	}
  }

  return nullptr;
}

// TODO: is this correct?
void op_comb_nullary() {
  op_comb_i();
}

// e.g.
// X == 102
// X [101 >] [2 /] [3 *] ifte
// -> 51
void op_comb_ifte() {
  if (cur_stack_size() <  4) {
	std::cout << "ifte expects 4 objects\n";
	return;
  }

  // [3 *]
  auto a = op_peek(0);
  // [2 /]
  auto b = op_peek(1);
  // [101 >] 
  auto c = op_peek(2);
  // X
  auto d = op_peek(3);

  if (a->type != LIST || b->type != LIST || c->type != LIST) {
	std::cout << "ifte expects three list objects!\n";
	return;
  }

  auto l_a = op_pop();
  auto l_b = op_pop();
  auto l_c = op_pop();
  auto d_o = op_pop();

  op_push(d_o, d_o->type);
  op_push(l_c, l_c->type);
  op_comb_i();

  auto res1 = op_get_head();
  if (get_bool(res1) == true) {	
	op_pop();
	op_push(d_o, d_o->type);
	op_push(l_b, l_b->type);
	op_comb_i();
	return;
  }

  op_pop();
  op_push(d_o, d_o->type);
  op_push(l_a, l_a->type);
  op_comb_i();
  return;
}

// TODO: strings? 
void op_comb_step() {
  if (cur_stack_size() <  2) {
	std::cout << "ifte expects 2 objects\n";
	return;
  }

  auto a = op_peek(0);
  auto b = op_peek(1);

  if (a->type != LIST || b->type != LIST) {
	std::cout << "ifte expects two list objects!\n";
	return;
  }

  auto l_a = op_pop();
  auto l_b = get_list(op_pop());

  for(int i = 1; i < l_b.size(); i++) {
	op_push(l_b[i], l_b[i]->type);
	op_push(l_a, l_a->type);
	op_comb_i();
  }

  return;
}

// TODO: Strings
void op_comb_filter() {
  if (cur_stack_size() <  2) {
	std::cout << "filter expects 2 objects\n";
	return;
  }

  auto a = op_peek(0);
  auto b = op_peek(1);

  if (a->type != LIST || (b->type != STR && b->type != LIST)) { 
	std::cout << "filter expects two list objects!\n";
	return;
  }

  // TODO: need some advanced type checking here for
  // the filtering operand. it should resolve to a bool type
  auto l_a = op_pop();
  auto temp_b = op_pop();

  if (temp_b->type == LIST) {
	std::vector<joy_object*> res_list;
	auto head = new joy_object(LIST);
	res_list.push_back(head);
	auto l_b = get_list(temp_b);
	for(int i = 1; i < l_b.size(); i++) {
	  op_push(l_b[i], l_b[i]->type);
	  op_push(l_a, l_a->type);
	  op_comb_i();
	  auto res = op_pop();
	  if (res->type != BOOL) {
		std::cout << "Type error: filter operand not resolving to a bool!\n";
		return;
	  }
	  if (get_bool(res) == true) {
		res_list.push_back(l_b[i]);
	  }
	}
	auto res_o = new joy_object(res_list);
	op_push(res_o, res_o->type);
  }
  return;
}

void op_comb_cleave() {
  if (cur_stack_size() <  3) {
	std::cout << "cleave expects 3 objects\n";
	return;
  }

  // [0 [+] fold] 
  auto a = op_peek(0);
  // [size]
  auto b = op_peek(1);
  // X
  auto c = op_peek(2);

  if (a->type != LIST || b->type != LIST) {
	std::cout << "cleave expects two list objects!\n";
	return;
  }

  auto l_a = op_pop();
  auto l_b = op_pop();
  auto c_o = op_pop();

  op_push(c_o, c_o->type);
  op_push(l_b,l_b->type);
  op_comb_i();

  op_push(c_o, c_o->type);
  op_push(l_a, l_a->type);
  op_comb_i();
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

//  std::unordered_map<std::string, op_ptr> builtins;
void setup_builtins() {
  //using op_ptr = joy_object (*)();
  /** math ops**/
  builtins["+"] = (voidFunction)op_add;
  builtins["*"] = (voidFunction)op_mul;
  builtins["/"] = (voidFunction)op_div;
  builtins["%"] = (voidFunction)op_rem;
  builtins["-"] = (voidFunction)op_sub;
  builtins["<"] = (voidFunction)op_lt;
  builtins[">"] = (voidFunction)op_gt;
  builtins[">="] = (voidFunction)op_gte;
  builtins["<="] = (voidFunction)op_lte;
  builtins["="] = (voidFunction)op_eq;
  builtins["!="] = (voidFunction)op_ne;
  builtins["or"] = (voidFunction)op_or;
  builtins["xor"] = (voidFunction)op_xor;
  builtins["and"] = (voidFunction)op_and;
  builtins["not"] = (voidFunction)op_not;
  builtins["neg"] = (voidFunction)op_neg;
  builtins["abs"] = (voidFunction)op_abs;
  builtins["acos"] = (voidFunction)op_acos;
  builtins["asin"] = (voidFunction)op_asin;
  builtins["atan"] = (voidFunction)op_atan;
  builtins["atan2"] = (voidFunction)op_atan2;
  builtins["ceil"] = (voidFunction)op_ceil;
  builtins["cos"] = (voidFunction)op_cos;
  builtins["cosh"] = (voidFunction)op_cosh;
  builtins["exp"] = (voidFunction)op_exp;
  builtins["floor"] = (voidFunction)op_floor;
  //builtins["frexp"] = (voidFunction)op_frexp;
  builtins["ldexp"] = (voidFunction)op_ldexp;
  builtins["log"] = (voidFunction)op_log;
  builtins["log10"] = (voidFunction)op_log10;
  //builtins["modf"] = (voidFunction)op_modf;
  builtins["pow"] = (voidFunction)op_pow;
  builtins["sin"] = (voidFunction)op_sin;
  builtins["sqrt"] = (voidFunction)op_sqrt;
  builtins["sinh"] = (voidFunction)op_sinh;
  builtins["tan"] = (voidFunction)op_tan;
  builtins["tanh"] = (voidFunction)op_tanh;
  builtins["trunc"] = (voidFunction)op_trunc;
  builtins["srand"] = (voidFunction)op_srand;
  builtins["pred"] = (voidFunction)op_pred;
  builtins["succ"] = (voidFunction)op_succ;
  builtins["max"] = (voidFunction)op_max;
  builtins["min"] = (voidFunction)op_min;
  builtins["maxint"] = (voidFunction)op_maxint;
  builtins["rand"] = (voidFunction)op_rand;

  /** files/strings etc... **/
  //builtins["fopen"] = (voidFunction)op_fopen;
  //builtins["fclose"] = (voidFunction)op_fclose;
  //builtins["feof"] = (voidFunction)op_feof;
  //builtins["ferror"] = (voidFunction)op_ferror;
  //builtins["fflush"] = (voidFunction)op_fflush;
  //builtins["ferror"] = (voidFunction)op_ferror;
  //builtins["fgetch"] = (voidFunction)op_fgetch;
  //builtins["fgets"] = (voidFunction)op_fgets;
  //builtins["fopen"] = (voidFunction)op_fopen;
  //builtins["fread"] = (voidFunction)op_fread;
  //builtins["fwrite"] = (voidFunction)op_fwrite;
  //builtins["fremove"] = (voidFunction)op_fwrite;
  //builtins["frename"] = (voidFunction)op_fwrite;
  //builtins["fput"] = (voidFunction)op_fwrite;
  //builtins["fputch"] = (voidFunction)op_fwrite;
  //builtins["fputchars"] = (voidFunction)op_fwrite;
  //builtins["fputstring"] = (voidFunction)op_fwrite;
  //builtins["fseek"] = (voidFunction)op_fwrite;
  //builtins["ftell"] = (voidFunction)op_fwrite;

  /** stack ops**/
  builtins["clear"] = (voidFunction)op_clear_stack;
  builtins["pop"] = (voidFunction)op_pop;
  builtins["dup"] = (voidFunction)op_dup;
  builtins["first"] = (voidFunction)op_first;
  builtins["rest"] = (voidFunction)op_rest;
  builtins["cons"] = (voidFunction)op_cons;
  builtins["swap"] = (voidFunction)op_swap;
  builtins["swons"] = (voidFunction)op_swons;
  builtins["rollup"] = (voidFunction)op_rollup;
  builtins["rolldown"] = (voidFunction)op_rolldown;
  builtins["rotate"] = (voidFunction)op_rotate;
  builtins["size"] = (voidFunction)op_size;
  builtins["small"] = (voidFunction)op_small;
  builtins["reverse"] = (voidFunction)op_reverse;
  builtins["at"] = (voidFunction)op_at;
  builtins["of"] = (voidFunction)op_of;
  builtins["null"] = (voidFunction)op_null;
  builtins["uncons"] = (voidFunction)op_uncons;
  builtins["unstack"] = (voidFunction)op_unstack;
  builtins["ord"] = (voidFunction)op_ord;
  builtins["chr"] = (voidFunction)op_chr;
  //builtins["setsize"] = (voidFunction)op_setsize;
  builtins["stack"] = (voidFunction)op_stack;
  builtins["id"] = (voidFunction)op_id;
  builtins["popd"] = (voidFunction)op_popd;
  builtins["dupd"] = (voidFunction)op_dupd;
  builtins["swapd"] = (voidFunction)op_swapd;
  builtins["rotated"] = (voidFunction)op_rotated;
  builtins["rollupd"] = (voidFunction)op_rollupd;
  builtins["rolldownd"] = (voidFunction)op_rolldownd;
  builtins["choice"] = (voidFunction)op_choice;
  builtins["compare"] = (voidFunction)op_compare;
  builtins["opcase"] = (voidFunction)op_opcase;
  builtins["case"] = (voidFunction)op_case;
  //builtins["drop"] = (voidFunction)op_compare;
  //builtins["take"] = (voidFunction)op_compare;
  //builtins["enconcat"] = (voidFunction)op_compare;
  //builtins["name"] = (voidFunction)op_compare;
  //builtins["intern"] = (voidFunction)op_compare;
  //builtins["body"] = (voidFunction)op_compare;
  //builtins["has"] = (voidFunction)op_compare;
  //builtins["in"] = (voidFunction)op_compare;
  //builtins["integer"] = (voidFunction)op_compare;
  //builtins["char"] = (voidFunction)op_compare;
  //builtins["logical"] = (voidFunction)op_compare;
  //builtins["set"] = (voidFunction)op_compare;
  //builtins["string"] = (voidFunction)op_compare;
  //builtins["list"] = (voidFunction)op_compare;
  //builtins["leaf"] = (voidFunction)op_compare;
  //builtins["user"] = (voidFunction)op_compare;
  //builtins["float"] = (voidFunction)op_compare;
  //builtins["file"] = (voidFunction)op_compare;
  
  /** combinators **/
  builtins["i"] = (voidFunction)op_comb_i;
  builtins["x"] = (voidFunction)op_comb_x;
  builtins["dip"] = (voidFunction)op_comb_dip;
  builtins["map"] = (voidFunction)op_comb_map;
  builtins["fold"] = (voidFunction)op_comb_fold;
  builtins["cleave"] = (voidFunction)op_comb_cleave;
  builtins["ifte"] = (voidFunction)op_comb_ifte;
  builtins["step"] = (voidFunction)op_comb_step;
  builtins["filter"] = (voidFunction)op_comb_filter;
  builtins["split"] = (voidFunction)op_comb_split;
  builtins["app1"] = (voidFunction)op_comb_app1;
  builtins["app11"] = (voidFunction)op_comb_app11;
  builtins["app12"] = (voidFunction)op_comb_app12;
  //builtins["construct"] = (voidFunction)op_comb_split;
  builtins["nullary"] = (voidFunction)op_comb_nullary;
  //builtins["unary"] = (voidFunction)op_comb_split;
  //builtins["unary2"] = (voidFunction)op_comb_split;
  //builtins["unary2"] = (voidFunction)op_comb_split;
  //builtins["unary3"] = (voidFunction)op_comb_split;
  //builtins["unary4"] = (voidFunction)op_comb_split;
  //builtins["binary"] = (voidFunction)op_comb_split;
  //builtins["ternary"] = (voidFunction)op_comb_split;
  //builtins["branch"] = (voidFunction)op_comb_split;
  //builtins["ifinteger"] = (voidFunction)op_comb_split;
  //builtins["ifchar"] = (voidFunction)op_comb_split;
  //builtins["iflogical"] = (voidFunction)op_comb_split;
  //builtins["ifset"] = (voidFunction)op_comb_split;
  //builtins["ifstring"] = (voidFunction)op_comb_split;
  //builtins["iflist"] = (voidFunction)op_comb_split;
  //builtins["iffloat"] = (voidFunction)op_comb_split;
  //builtins["iffile"] = (voidFunction)op_comb_split;
  //builtins["cond"] = (voidFunction)op_comb_split;
  //builtins["while"] = (voidFunction)op_comb_split;
  //builtins["linerec"] = (voidFunction)op_comb_split;
  //builtins["tailrec"] = (voidFunction)op_comb_split;
  //builtins["binrec"] = (voidFunction)op_comb_split;
  //builtins["genrec"] = (voidFunction)op_comb_split;
  //builtins["condnestrec"] = (voidFunction)op_comb_split;
  //builtins["condlinerec"] = (voidFunction)op_comb_split;
  //builtins["times"] = (voidFunction)op_comb_split;
  //builtins["infra"] = (voidFunction)op_comb_split;
  //builtins["primerec"] = (voidFunction)op_comb_split;
  //builtins["some"] = (voidFunction)op_comb_split;
  //builtins["all"] = (voidFunction)op_comb_split;
  //builtins["treestep"] = (voidFunction)op_comb_split;
  //builtins["treerec"] = (voidFunction)op_comb_split;
  //builtins["treegenrec"] = (voidFunction)op_comb_split;

  /** misc **/
  //builtins["help"] = (voidFunction)op_comb_split;
  //builtins["manual"] = (voidFunction)op_comb_split;
  //builtins["include"] = (voidFunction)op_comb_split;
  //builtins["quit"] = (voidFunction)op_comb_split;
}

/** PARSER **/
bool peek(std::string::const_iterator i,
				 std::string *input, std::string match) {
  // TODO: fix this
  while(i != input->end()){
	if (!std::isspace(*i))
	  break;
	i++;
  }
  if (input->compare(i - input->begin(), match.length(), match) == 0)
	return true;
  return false;
}



std::string::const_iterator
eat_space(std::string* input, std::string::const_iterator i) {
  while (i != input->end() && (std::isspace(*i))) {
	i++;
  }
  return i;
}

std::tuple<std::string::const_iterator, joy_object*, bool>
parse_numeric(std::string::const_iterator i, std::string* input) {
  std::string cur_num;
  bool is_float = false;
  while(i != input->end()) {
	if (*i == '.')
	  is_float = true;

	if ((*i != '.') && !std::isdigit(*i)) { 
	  break;
	}

	cur_num += *i;  
	i++;
  }

  if (!is_float) {
	auto as_int = (int64_t) stoi(cur_num);
	auto o = new joy_object(as_int);
	//op_push(o, INT);
	return {i,o, is_float};
  }
  else {
	auto as_float = (float) stof(cur_num);
	auto o = new joy_object(as_float);
	return {i,o, is_float};
  }
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
	i = eat_space(input, i);
	if (peek(i, input, "==")) {
	  is_def = true;
	  i+=2;
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
  bool is_float;

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
	  std::tie(it, o, is_float) = parse_numeric(it, input);
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
	  //std::cout << "PARSED_LIST" << std::endl;
	  cur_list.push_back(o);
	  ++it;// ?
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
		//std::cout << "PARSING IDENT IN LIST\n";
		std::tie(it, o, is_def, user_ident) = parse_ident(it, input);
		cur_list.push_back(o);
	  }
	}
	// TODO: more ops here
	else if (*it == '+' || *it == '*' || *it == '/' || *it == '>' || *it == '<'
			 || *it == '=' || *it == '-' || std::isalpha(*it)) {
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

void print_data(std::variant<int64_t,
				float, std::string, bool,
				std::vector<joy_object*>> data, joy_object* o, bool print_space = true) {
 	switch(data.index())
	  {
	  case 0:
		std::cout << std::get<int64_t>(data);
		break;
	  case 1:
		std::cout << std::fixed << std::setprecision(2) << std::get<float>(data);
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
	if (stck[temp_ptr] == nullptr) {
	  temp_ptr--;
	  continue;
	}
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
  bool is_float = false;
  while(it != input.end()){	
	joy_object* o = nullptr;
	if (std::isdigit(*it)) {
	  //it = std::get<std::string::const_iterator>(parse_numeric(it, &input));
	  std::tie(it, o, is_float) = parse_numeric(it, &input);
	  if (is_float)
		op_push(o, FLOAT, cur_stack);
	  else
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
	else if (*it == '+' || *it == '*' || *it == '/' || *it == '='
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

void interpret_file(std::string file_path) {
  std::ifstream in_file(file_path);
  std::string line;

  if (in_file.is_open()) {
	while (std::getline(in_file, line, ';')) {
	  // std::cout << line << std::endl;
	  parse_line(line);
	}
  }
  
}

int main(int argc, char *argv[]) {
  printf("RUNNING JOYVM\n");
  setup_builtins();
  if (argc > 1) {
	std::string in = (std::string) argv[1];
	interpret_file(in);
  }
  run_interpreter();
}
