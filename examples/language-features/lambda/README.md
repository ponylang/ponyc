# lambda

A program showing how to create and call lambda functions.

## How to Compile

With a minimal Pony installation, in the same directory as this README file run `ponyc`. You should see content building the necessary packages, which ends with:

```console
...
Generating
 Reachability
 Selector painting
 Data prototypes
 Data types
 Function prototypes
 Functions
 Descriptors
Optimising
Writing ./lambda.o
Linking ./lambda
```

## How to Run

Once `lambda` has been compiled, in the same directory as this README file run `./lambda`. You should see a three line output showing the results of our lambdas.

```console
$ ./lambda
Add: 9
Mult: 18
Error: 0
```

What is not immediately obvious while looking at our results or at the program itself is that Pony lambdas, just like all things in Pony, are objects. Lambdas are object literals with a single `apply` function defined the same as the lambda.

```pony
{(a: U32, b: U32): U32 => a + b }
```

is rewritten by the compiler to

```pony
object
  fun apply(a: U32, b: U32): U32 => a + b
end
```

The lambda syntax is often cleaner to read, while the object literal syntax is equivalent in functionality. Use whichever fits your needs!

## Program Modifications

Modify the program to replace each lambda with its object literal equivalent. The output should not change!

```console
$ ./lambda
Add: 9
Mult: 18
Error: 0
```
