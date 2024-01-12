A fairly straightforaward implementation of the Joy programming language.

Joy is a stack based language (think Forth), with some higher level features like combinators, maps, and folds (think Haskell).

Here are some small examples, try typing them in after compiling and running joy.cpp

```
joy~> X == 1 
joy~> 3 X +

joy~> 1 [X =] i 

joy~> [1 2 3 4] [X +] map
```

compile with: g++ joy.cc --std=c++17 -o joy (or just type make). Tested on Linux and macOS.

This is still very much a work in progress. Not all primitives are implemented yet, but most of the examples in the tutorial should work fine.


