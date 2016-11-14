numbers-c
=========

Countdown numbers game solver written in C.

### Build

```
git clone https://github.com/panzi/numbers-c.git
cd numbers-c
make
```

### Usage

```
./build/numbers <threads> <target> [<number>...]
./build/numbers - <target> [<number>...]
```

Passing `-` for the number of threads will try to detect the number of CPUs
(cores) on your system and use that. This is currently only supported on
Unix systems that support `sysconf(_SC_NPROCESSORS_ONLN)` (e.g. recent Linux;
FreeBSD and Mac OS X also support this, but I haven't tested those systems).

### Number Game Rules

In this "given number" doesn't refer to a certain value of a number, but to
a number as given in the problem set. The same value may occur more than once
in the problem set.

 * all given numbers are positive integers
 * each given number may be used at most once
 * allowed operations are addition, subtraction, multiplication, and division
 * at no intermediate step in the process can the current running total become
   negative or involve a fraction
 * use all of this to produce a given target number

In the original game from the TV show only certain given numbers are allowed,
there are always 6 of them, and the target number is in the range 101 to 999.
But this program can take any positive integer (`unsigned long`) for any number
and the target. Given enough RAM and CPU time it supports up to 64 given numbers
on a 64bit machine (32 on a 32bit machine).

### Algorithm

The simplest algorithm to solve a number game is:

 * define a list of intermediate expressions
 * define a list of results
 * for all given numbers, check if they are already the target number
  * if yes, put those into the list of results
  * if no, put those into the list of intermediate expressions
 * repeat for as long as new expressions are generated
  * combine all expressions generated in the last iteration with all other
    expressions (in the first iteration combine all single-number-expressions
    with each other), but only if the new expression won't violate any of the
    rules from the previous section
  * check if the new expression is equal to the target number
   * if it is, put it into the list of results
   * if not, put it into the list of intermediate expressions

This is of course slow. Several optimizations to this are done in this program.

First the check whether two intermediate expressions only use mutually exclusive
given numbers is done by comparing bit-fields that are attached to each
expression (called `used`). Each bit in those stands for a given number that is
used in the expression so that during combination the expression
`(a->used & b->used) == 0` to test if the two expressions don't use the same
numbers.

Rules for generated expressions are more strict than with the original number
game, excluding useless operations like `1 * x` or `x / 1` (fewer generated
expressions equals faster).

Addition and multiplication are commutative, which means it doesn't make sense
to put in `a + b` and `b + a`. In fact the actually generated expressions are
filtered for a certain normal form that I have came up with, which makes the
results unique and again reduces the number of generated expressions. To make
clear what I mean an example: All of these expressions are equivalent even
though minus is not commutative, so only one of them is used.

 * `((75 - 50) - 5) - 1`
 * `((75 - 5) - 50) - 1`
 * `((75 - 50) - 1) - 5`
 * `((75 - 1) - 50) - 5`
 * `((75 - 5) - 1) - 50`
 * `((75 - 1) - 5) - 50`

See [Normalization Rules](#normalization-rules).

Iterating through all other expressions in the combination step is again slow.
So instead one can group all expressions by the numbers occurring in them and
then only iterating over the sets of expressions that are fully distinct to the
current expression.

This can be done by interpreting `expr->used - 1` as an index into an array of
an array of expressions. New expressions are put into the corresponding array
and then in the combination step only the sets that are fully distinct to
the current expression are looked at.

The whole purpose of the `- 1` is so there is no empty set at index `0` that is
never used (because only expressions that don't use any given numbers would be
put there).

The next optimization is to split the expressions generated in the previous
iteration into equally sized parts and combine them with all other generated
expression in parallel threads, one thread per CPU core.

### Normalization Rules

These rules are used to normalize the expressions. Actually since the algorithm
tries to combine all already existing expressions with each other these rules
are used to only accept new expressions when they happen to be normalized.

```
IsNormalizedAddition(left, right) =>
	match right =>
		(_ + _) => false
		(_ - _) => false
		_       =>
			match left =>
				(_ + x) => x <= right
				(_ - _) => false
				_       => left <= right

IsNormalizedSubtraction(left, right) =>
	match right =>
		(_ + _) => false
		(_ - _) => false
		_       =>
			match left =>
				(_ - x) => x <= right
				_       => true

IsNormalizedMultiplication(left, right) =>
	match right =>
		(_ * _) => false
		(_ / _) => false
		_       =>
			match left =>
				(_ * x) => x <= right
				(_ / _) => false
				_       => left <= right

IsNormalizedDivision(left, right) =>
	match right =>
		(_ * _) => false
		(_ / _) => false
		_       =>
			match left =>
				(_ / x) => x <= right
				_       => true
```

These rules allow sequences of additions/substractions and sequences of
multiplications/divisions to only nest on the left side, producing `(a + b) + c`
instead of `a + (b + c)` and it ensures sorts the expressions with the bigger
values to the right, producing `1 + 2 + 3` instead of `3 + 2 + 1`.

### Similar programs in other languages

 * https://github.com/panzi/numbers-python
 * https://github.com/panzi/numbers-js
 * https://github.com/panzi/numbers-rust
