# circle

Define, instantiate, and summarize a set of circles. Our `Circle` class only requires defining the radius of the circle and from that radius we are able to calculate the area and circumference of the corresponding circle.

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
Writing ./circle.o
Linking ./circle
```

## How to Run

Once `circle` has been compiled, in the same directory as this README file run `./circle`. You should see a series of circle definitions printed, which ends with:

```console
$ ./circle
...
Radius: 98
Circumference: 615.752
Area: 30171.9

Radius: 99
Circumference: 622.035
Area: 30790.8

Radius: 100
Circumference: 628.319
Area: 31415.9
```

## Program Modifications

Modify the program to add a `get_diameter()` method and print the diameter alongside the other measures -- diameter is two times the radius.

```console
$ ./circle
...
Radius: 98
Diameter: 196
Circumference: 615.752
Area: 30171.9

Radius: 99
Diameter: 198
Circumference: 622.035
Area: 30790.8

Radius: 100
Diameter: 200
Circumference: 628.319
Area: 31415.9
```

For further modification, modify the program so that `get_circumference()` is calculated using your `get_diameter()` method.
