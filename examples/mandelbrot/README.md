# mandelbrot

A program showing an example of how to implement a divide-and-conquer algorithm to plot a Mandelbrot set.

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
Writing ./mandelbrot.o
Linking ./mandelbrot
```

## How to Run

Once `mandelbrot` has been compiled, in the same directory as this README file run `./mandelbrot --output output.pbm`. After the program finishes executing, you should see that a file named `output.pbm` has been created in the same directory.

If you have a suitable Netpbm viewer program, you can open this file directly. If you don't, you can use the `convert` tool (included in the ImageMagick Tools) to convert the output to a PNG image, using the following command:

```console
convert output.pbm PNG:output.png
```

You should now be able to open the PNG image, and see a depiction of a Mandelbrot set.

## Program Modifications

The program accepts different command-line arguments to modify the final image. You can run `./mandelbrot --help` to see them all:

```console
./mandelbrot --help

usage: run [<options>] [<args> ...]

Plot a Mandelbrot set using a divide-and-conquer algorithm.

The output can be viewed directly with a suitable Netpbm viewer, or converted
to a PNG with the following command
(ImageMagick Tools required):

  convert <output>.pbm PNG:<output>.png

Options:
   -i, --iterations=50    Maximum amount of iterations to be done for each pixel.
   -w, --width=16000      Lateral length of the resulting mandelbrot image.
   -c, --chunks=16        Maximum line count of chunks the image should be divided into for divide & conquer processing.
   -h, --help=false
   -o, --output=          File to write the output to.
   -l, --limit=4          Square of the limit that pixels need to exceed in order to escape from the Mandelbrot set.
```

You can tune the number of chunks (using the `--chunks` flag) that the image will be divided into, and see how it affects the time it takes for the program to finish.
