# CS430P4
Fourth Project for Computer Graphics Course

Continuation of CS430P3, with the inclusion of reflections and refraction.

NOTE: In order to use the math library, Linux requires -lm be appended to the compile command, if you are unable to compile it, try removing it from the command in the makefile as follows: 

gcc raytrace.c -o raytrace

Otherwise, simply call make to compile, then run program by calling:
./raytrace width height input.json output.ppm

where width and height are dimensions for the image to be created in the file specified at output.ppm. 
Input and output files can be named by user, so long as they are json and ppm files, respectively.

Objects within the json that have duplicate values (such as two color keys or two camera objects) will be overwritten by
objects later in the file.

*NOTE: Currently IoR is not implemented. Currently every other portion is implemented. Will remove this note if IoR is implemented prior to deadline.*