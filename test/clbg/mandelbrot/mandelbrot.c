// gcc -pipe -Wall -O3 -fomit-frame-pointer -march=native -std=c99 -D_GNU_SOURCE
//     -mfpmath=sse -msse2 -fopenmp mandelbrot.c
//
// ./a.out 16000 >> output.txt
//  convert output.txt -background black -alpha remove -alpha off -colors 16
//                     -compress none BMP2:output.bmp

// The Computer Language Benchmarks Game
// http://benchmarksgame.alioth.debian.org/
//
// Contributed by Jeremy Zerfas

// This is the square of the limit that pixels will need to exceed in order to
// escape from the Mandelbrot set.
#define LIMIT_SQUARED      4.0
// This controls the maximum amount of iterations that are done for each pixel.
#define MAXIMUM_ITERATIONS   50

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

// intptr_t should be the native integer type on most sane systems.
typedef intptr_t intnative_t;

int main(int argc, char ** argv){
   // Ensure image_Width_And_Height are multiples of 8.
   const intnative_t image_Width_And_Height=(atoi(argv[1])+7)/8*8;

   // The image will be black and white with one bit for each pixel. Bits with
   // a value of zero are white pixels which are the ones that "escape" from
   // the Mandelbrot set. We'll be working on one line at a time and each line
   // will be made up of pixel groups that are eight pixels in size so each
   // pixel group will be one byte. This allows for some more optimizations to
   // be done.
   uint8_t * const pixels=malloc(image_Width_And_Height*
     image_Width_And_Height/8);

   // Precompute the initial real and imaginary values for each x and y
   // coordinate in the image.
   double initial_r[image_Width_And_Height], initial_i[image_Width_And_Height];
   #pragma omp parallel for
   for(intnative_t xy=0; xy<image_Width_And_Height; xy++){
      initial_r[xy]=2.0/image_Width_And_Height*xy - 1.5;
      initial_i[xy]=2.0/image_Width_And_Height*xy - 1.0;
   }

   #pragma omp parallel for schedule(guided)
   for(intnative_t y=0; y<image_Width_And_Height; y++){
      const double prefetched_Initial_i=initial_i[y];
      for(intnative_t x_Major=0; x_Major<image_Width_And_Height; x_Major+=8){

         // pixel_Group_r and pixel_Group_i will store real and imaginary
         // values for each pixel in the current pixel group as we perform
         // iterations. Set their initial values here.
         double pixel_Group_r[8], pixel_Group_i[8];
         for(intnative_t x_Minor=0; x_Minor<8; x_Minor++){
            pixel_Group_r[x_Minor]=initial_r[x_Major+x_Minor];
            pixel_Group_i[x_Minor]=prefetched_Initial_i;
         }

         // Assume all pixels are in the Mandelbrot set initially.
         uint8_t eight_Pixels=0xff;

         intnative_t iteration=MAXIMUM_ITERATIONS;
         do{
            uint8_t current_Pixel_Bitmask=0x80;
            for(intnative_t x_Minor=0; x_Minor<8; x_Minor++){
               const double r=pixel_Group_r[x_Minor];
               const double i=pixel_Group_i[x_Minor];

               pixel_Group_r[x_Minor]=r*r - i*i +
                 initial_r[x_Major+x_Minor];
               pixel_Group_i[x_Minor]=2.0*r*i + prefetched_Initial_i;

               // Clear the bit for the pixel if it escapes from the
               // Mandelbrot set.
               if(r*r + i*i>LIMIT_SQUARED)
                  eight_Pixels&=~current_Pixel_Bitmask;

               current_Pixel_Bitmask>>=1;
            }
         }while(eight_Pixels && --iteration);

         pixels[y*image_Width_And_Height/8 + x_Major/8]=eight_Pixels;
      }
   }

   // Output the image to stdout.
   fprintf(stdout, "P4\n%jd %jd\n", (intmax_t)image_Width_And_Height,
     (intmax_t)image_Width_And_Height);
   fwrite(pixels, image_Width_And_Height*image_Width_And_Height/8, 1, stdout);

   free(pixels);

   return 0;
}
