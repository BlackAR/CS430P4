/*
 ============================================================================
 Name        : raytrace.c
 Author      : Anthony Black
 Description : CS430 Project 4: Ray Tracing
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

//#define DEBUG 1 //uncomment to see print statements
#define MAX_DEPTH 7   
//STRUCTURES
// Plymorphism in C
typedef struct Object{
  int type; // 0 = sphere, 1 = plane
  double position[3];
  double diffuse_color[3];
  double specular_color[3];
  double reflectivity	;
  double refractivity;
  double ior;
  union {
    struct {
      double radius;
    } sphere;
    struct {
      double normal[3];
    } plane;
  };
} Object;

typedef struct Camera{
  double width;
  double height;
} Camera;

typedef struct Light{
  double color[3];
  double position[3];
  double direction[3];
  double radial_a0;
  double radial_a1;
  double radial_a2;
  double theta;
  double angular_a0;
} Light;

typedef struct Pixel{
  unsigned char r, b, g;
}Pixel;

typedef struct Closest{
	Object* closest_object;
	double closest_t;
}Closest;

//PROTOTYPE DECLARATIONS 

//--------------JSON READING FUNCTIONS----------------------

int next_c(FILE* json);

void expect_c(FILE* json, int d);

void skip_ws(FILE* json);

char* next_string(FILE* json);

double next_number(FILE* json);

double* next_vector(FILE* json);

void read_scene(char* filename, Camera* camera, Object** objects, Light** lights);

//--------------VECTOR FUNCTIONS----------------------
void vector_normalize(double* v);

double vector_dot_product(double *v1, double *v2);

void vector_cross_product(double *v1, double *v2, double *result);

double vector_length(double *vector);

void vector_reflection(double *N, double *L, double *result);

void vector_subtraction(double *v1, double *v2, double *result);

void vector_addition(double *v1, double *v2, double *result);

void vector_scale(double *vector, double scalar, double *result);

//--------------INTERSECTION FUNCTIONS----------------------

double sphere_intersection(double* Ro, double* Rd, double* C, double r);

double plane_intersection(double* Ro, double* Rd, double* P, double* N);

//--------------LIGHT FUNCTIONS----------------------

double calculate_diffuse(double object_diff_color, double light_color, double *N, double *L);

double calculate_specular(double *L, double *N, double *R, double *V, double object_spec_color, double light_color);

double frad(Light * light, double t);

double fang(Light *light, double *L);

//--------------IMAGE FUNCTIONS----------------------

void generate_scene(Camera* camera, Object** objects, Light** lights, Pixel* buffer, int width, int height);

void write_p3(Pixel *buffer, FILE *output_file, int width, int height, int max_color);

double clamp(double value);

Closest* shoot(double* Ro, double* Rd, Object **objects);

Pixel* recursive_shade(Object **objects, Light **lights, double* Ro, double* Rd, Closest* current_object, int depth);

//===========================================================================================================

//Global variable for tracking during reading of JSON file, to report errors.
int line = 1;

//FUNCTIONS

int main(int argc, char *argv[]) {
	/*
	inputs:
		int argc: the number of arguments in argv[]. Should be 5.
		char *argv[]: the arguments in this should be: 
		  filepath (implicit)
		  width (a number value greater than 0)
		  height (a number value greater than 0)
		  input filename of JSON file (must exist)
		  output filename of PPM file (does not need to exist)
	output:
		void
	function:
		this program takes in a JSON file and generates an image to
		the filename passed in with the given width and height.
	*/
  
  #ifdef DEBUG
    printf("Checking arguments...\n");
  #endif
  //ensures the correct number are passed in
  if (argc != 5){
    fprintf(stderr, "Error: Insufficient Arguments. Arguments provided: %d.\n", argc);
  }
  #ifdef DEBUG
    printf("Getting width and height...\n");
  #endif
  int width = atoi(argv[1]);
  int height = atoi(argv[2]);
  //check for positive width
  if (width <= 0){
    fprintf(stderr, "Error: Non-positive width provided.\n");
  }
  //check for positive height
  if (height <= 0){
    fprintf(stderr, "Error: Non-positive height provided.\n");
  }
  //create array of objects
  #ifdef DEBUG
    printf("Allocating memory...\n");
  #endif
  Object **objects;
  objects = malloc(sizeof(Object*)*128);
  //create camera object
  Camera *camera;
  camera = (Camera *)malloc(sizeof(Camera));
  //create array of lights
  Light **lights;
  lights = malloc(sizeof(Light*)*128);
  //create buffer for image
  Pixel *buffer; 
  buffer = (Pixel *)malloc(width*height*sizeof(Pixel));
  #ifdef DEBUG
    printf("Reading scene...\n");
  #endif
  read_scene(argv[3], camera, objects, lights);
  #ifdef DEBUG
    printf("Generating scene...\n");
  #endif
  generate_scene(camera, objects, lights, buffer, width, height);
  #ifdef DEBUG
    printf("Opening output file...\n");
  #endif
  FILE* output_file = fopen(argv[4], "w");
  //error handling for failure to open output file
  if (output_file == NULL){
    fprintf(stderr, "Error: Unexpectedable to open output file.\n");
    fclose(output_file);
    exit(1);
  }
  #ifdef DEBUG
    printf("Creating image...\n");
  #endif
  write_p3(buffer, output_file, width, height, 255);
  fclose(output_file);
  //free memory
  //objects
  for(int i = 0; lights[i]!=NULL; i+=1){
  	free(objects[i]);
  }
  free(objects);
  //lights
  for(int i = 0; lights[i]!=NULL; i+=1){
  	free(lights[i]);
  }
  free(lights);
  //camera
  free(camera);
  return EXIT_SUCCESS;
}

//--------------JSON READING FUNCTIONS----------------------

int next_c(FILE *json) {
	/*
	inputs: 
		FILE *json: the JSON file
	output:
		int: value of character
	function:
		next_c() wraps the getc() function and provides error checking and line
  	number maintenance
	*/
  int c = fgetc(json);
  #ifdef DEBUG
    printf("next_c: '%c'\n", c);
  #endif
  if (c == '\n') {
    line += 1;
  }
  if (c == EOF) {
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    fclose(json);
    exit(1);
  }
  return c;
}


void expect_c(FILE *json, int d) {
	/*
	inputs:
		FILE *json: the JSON file to be read
		int d: the integer value of the expected character to read
	output:
		void
	function:
		expect_c() checks that the next character is d.  If it is not it emits
  	an error.
	*/
  int c = next_c(json);
  if (c == d) return;
  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  fclose(json);
  exit(1);    
}


void skip_ws(FILE *json) {
	/*
	inputs:
		FILE *json: the JSON file to be read
	output:
		void
	function:
		skip_ws() skips white space in the file.
	*/
  int c = next_c(json);
  while (isspace(c)) {
    c = next_c(json);
  }
  ungetc(c, json);
}

char* next_string(FILE *json) {
	/*
	inputs:
		FILE *json: the JSON file to be read
	output:
		char*: pointer to the string read
	function:
		next_string() gets the next string from the file handle and emits an error
  	if a string can not be obtained. 
	*/
  char buffer[129];
  int c = next_c(json);
  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    fclose(json);
    exit(1);
  }  
  c = next_c(json);
  int i = 0;
  while (c != '"') {
    if (i >= 128) {
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
      fclose(json);
      exit(1);      
    }
    if (c == '\\') {
      fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
      fclose(json);
      exit(1);      
    }
    if (c < 32 || c > 126) {
      fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
      fclose(json);
      exit(1);
    }
    buffer[i] = c;
    i += 1;
    c = next_c(json);
  }
  buffer[i] = 0;
  return strdup(buffer);
}

double next_number(FILE *json) {
	/*
	inputs:
		FILE *json: the JSON file to be read
	output:
		double: number read from file
	function:
		next_number() reads the next number in the JSON
	*/
  double value;
  int count = fscanf(json, "%lf", &value);
  if (count != 1){
    fprintf(stderr, "Error: Failed to read number on line %d.\n", line);
    fclose(json);
    exit(1);
  }
  return value;
}

double* next_vector(FILE *json) {
	/*
	inputs:
		FILE *json: The JSON file to be read
	output:
		double*: pointer to the vector read in
	function:
		next_vector() reads in the next 3D vector from the JSON
		and returns it as a pointer to a double array of size 3.
	*/
  double* v = malloc(3*sizeof(double));
  expect_c(json, '[');
  skip_ws(json);
  v[0] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[1] = next_number(json);
  skip_ws(json);
  expect_c(json, ',');
  skip_ws(json);
  v[2] = next_number(json);
  skip_ws(json);
  expect_c(json, ']');
  return v;
}

void read_scene(char *filename, Camera *camera, Object **objects, Light **lights) {
  /*
	inputs:
		char *filename: name of JSON file to be read
		Camera *camera: Camera object to store the viewpointof the picture
		Object **objects: Object array to store spheres and planes read from JSON
		Light **lights: Light array to store lights read 
	output:
		void
	function:
		read_scene() reads a JSON file and stores:
		1 Camera
		up to 128 Objects
		up to 128 Lights
	*/
  int c;
  int current_light = -1;
  int current_item = -1; //for tracking the current object in the Object array
  int current_type; //for tracking the current object we are reading from the json list
  FILE* json = fopen(filename, "r");
  //if file does not exist
  if (json == NULL) {
    fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
    fclose(json);
    exit(1);
  }
  
  skip_ws(json);
  
  // Find the beginning of the list
  expect_c(json, '[');

  skip_ws(json);

  // Find the objects

  while (1) {
    c = fgetc(json);
    if (c == ']') {
      fprintf(stderr, "Error: This is the worst scene file EVER.\n");
      fclose(json);
      return;
    }
    if (c == '{') {
      skip_ws(json);
    
      // Parse the object
      char* key = next_string(json);
      if (strcmp(key, "type") != 0) {
        fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
        exit(1);
      }

      skip_ws(json);

      expect_c(json, ':');

      skip_ws(json);

      char* value = next_string(json);
      if(strcmp(value, "camera") == 0){
        current_type = 0;
      } 
      else if(strcmp(value, "sphere") == 0) {
        current_item++;
        if(current_item>127){
          fprintf(stderr, "Error: Too many objects in JSON. Program can only handle 128 objects.\n");
          fclose(json);
          exit(1);
        }
        objects[current_item] = malloc(sizeof(Object));
        objects[current_item]->type = 0;
        objects[current_item]->reflectivity = 0;
        current_type = 1;
      } 
      else if (strcmp(value, "plane") == 0) {
        current_item++;
        if(current_item>127){
          fprintf(stderr, "Error: Too many objects in JSON. Program can only handle 128 objects.\n");
          fclose(json);
          exit(1);
        }
        objects[current_item] = malloc(sizeof(Object));
        objects[current_item]->type = 1;
        objects[current_item]->reflectivity = 0;
        current_type = 2;
      }
      else if (strcmp(value, "light") == 0) {
        current_light++;
        if(current_light>127){
          fprintf(stderr, "Error: Too many lights in JSON. Program can only handle 128 objects.\n");
          fclose(json);
          exit(1);
        }
        lights[current_light] = malloc(sizeof(Light));
        current_type = 3;
      } 
      else { 
        fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
        fclose(json);
        exit(1);
      }
      skip_ws(json);

      while (1) {
        // , }
        c = next_c(json);
        if (c == '}') {
          // stop parsing this object
          break;
        } 
        else if (c == ',') {
          // read another field
          skip_ws(json);
          char* key = next_string(json);
          skip_ws(json);
          expect_c(json, ':');
          skip_ws(json);
          if (strcmp(key, "width") == 0){
            if(current_type == 0){  //only camera has width
              camera->width = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Current object type has width value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "height") == 0){
            if(current_type == 0){  //only camera has height
              camera->  height = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Current object type has height value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if (strcmp(key, "radial-a2") == 0){
            if(current_type == 3){
              lights[current_light]->radial_a2 = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Non-light type has radial-a2 value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if (strcmp(key, "radial-a1") == 0){
            if(current_type == 3){
              lights[current_light]->radial_a1 = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Non-light type has radial-a1 value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if (strcmp(key, "radial-a0") == 0){
            if(current_type == 3){
              lights[current_light]->radial_a0 = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Non-light type has radial-a0 value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if (strcmp(key, "angular-a0") == 0){
            if(current_type == 3){
              lights[current_light]->angular_a0 = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Non-light type has angular-a0 value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "radius") == 0){
            if(current_type == 1){  //only spheres have radius
              objects[current_item]->sphere.radius = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Current object type cannot have radius value! Detected on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }     
          else if(strcmp(key, "diffuse_color") == 0){ 
            if(current_type == 1 || current_type == 2){  //only spheres and planes have diffuse color
                double* vector = next_vector(json);
                objects[current_item]->diffuse_color[0] = vector[0];
                objects[current_item]->diffuse_color[1] = vector[1];
                objects[current_item]->diffuse_color[2] = vector[2];
                free(vector);
            }
            else{
              fprintf(stderr, "Error: Non-object type has color value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "specular_color") == 0){ 
            if(current_type == 1 || current_type == 2){  //only spheres and planes have specular color
                double* vector = next_vector(json);
                objects[current_item]->specular_color[0] = vector[0];
                objects[current_item]->specular_color[1] = vector[1];
                objects[current_item]->specular_color[2] = vector[2];
                free(vector);
            }
            else{
              fprintf(stderr, "Error: Non-object type has color value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "reflectivity") == 0){
            if(current_type == 1 || current_type == 2){  //only spheres and planes have specular color
                objects[current_item]->reflectivity = next_number(json);
                int value = objects[current_item]->reflectivity + objects[current_item]->refractivity;
                if (value > 1){
                	fprintf(stderr, "Error: Sum of refractivity and reflectivity of object exceed 1 on line: %d.\n", line);
              		fclose(json);
              		exit(1);

                }
            }
            else{
              fprintf(stderr, "Error: Non-object type has reflectivity value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "refractivity") == 0){ 
            if(current_type == 1 || current_type == 2){  //only spheres and planes have specular color
                objects[current_item]->refractivity = next_number(json);
                int value = objects[current_item]->reflectivity + objects[current_item]->refractivity;
                if (value > 1){
                	fprintf(stderr, "Error: Sum of refractivity and reflectivity of object exceed 1 on line: %d.\n", line);
              		fclose(json);
              		exit(1);

                }
            }
            else{
              fprintf(stderr, "Error: Non-object type has refractivity value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "ior") == 0){ 
            if(current_type == 1 || current_type == 2){  //only spheres and planes have specular color
                objects[current_item]->ior = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Non-object type has IoR value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "color") == 0){ 
            if(current_type == 3){  //only lights have color
                double* vector = next_vector(json);
                lights[current_light]->color[0] = vector[0];
                lights[current_light]->color[1] = vector[1];
                lights[current_light]->color[2] = vector[2]; 
                free(vector);
            }
            else{
              fprintf(stderr, "Error: Non-light type has color value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          } 
          else if(strcmp(key, "position") == 0){
            if(current_type == 1 || current_type == 2){  //only spheres and planes have position
              double* vector = next_vector(json);
              objects[current_item]->position[0] = vector[0];
              objects[current_item]->position[1] = vector[1];
              objects[current_item]->position[2] = vector[2];
              free(vector);
            }
            else if(current_type == 3){  //only spheres and planes have position
              double* vector = next_vector(json);
              lights[current_light]->position[0] = vector[0];
              lights[current_light]->position[1] = vector[1];
              lights[current_light]->position[2] = vector[2];  
              free(vector);
            }
            else{
              fprintf(stderr, "Error: Camera type has position value on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          } 
          else if(strcmp(key, "normal") == 0){
            if(current_type == 2){  //only planes have normal
              double* vector = next_vector(json);
              objects[current_item]->plane.normal[0] = vector[0];
              objects[current_item]->plane.normal[1] = vector[1];
              objects[current_item]->plane.normal[2] = vector[2];  
              free(vector);
            }
            else{
              fprintf(stderr, "Error: Only planes have normal values on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "direction") == 0){
            if(current_type == 3){  //only planes have normal
              double* vector = next_vector(json);
              lights[current_light]->direction[0] = vector[0];
              lights[current_light]->direction[1] = vector[1];
              lights[current_light]->direction[2] = vector[2];  
              free(vector);
            }
            else{
              fprintf(stderr, "Error: Only planes have normal values on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          }
          else if(strcmp(key, "theta") == 0){
            if(current_type == 3){  //only spheres have radius
              lights[current_light]->theta = next_number(json);
            }
            else{
              fprintf(stderr, "Error: Current object type cannot have theta value! Detected on line number %d.\n", line);
              fclose(json);
              exit(1);
            }
          } 
          else{
            fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                key, line);
            fclose(json);
            exit(1);
            //char* value = next_string(json);
          }

          skip_ws(json);
        } 
        else {
          fprintf(stderr, "Error: Unexpected value on line %d\n", line);
          fclose(json);
          exit(1);
        }
      }

      skip_ws(json);

      c = next_c(json);

      if (c == ',') {
        // noop
        skip_ws(json);
      } 
      else if (c == ']') {
        fclose(json);
        return;
      } 
      else {
        fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
        fclose(json);
        exit(1);
      }
    }
  }
}

//--------------VECTOR FUNCTIONS----------------------

void vector_normalize(double *v) {
	/*
	inputs:
		double *v: vector to be normalize
	output:
		void
	function:
		vector_normalize() divides a vector by it's length to get a unit vector
	*/
  double len = sqrt(pow(v[0], 2) + pow(v[1], 2) + pow(v[2], 2));
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}

double vector_dot_product(double *v1, double *v2){
	/*
	inputs:
		double *v1: 3D vector
		double *v2: 3D vector
	output:
		double: dot product of the two vectors
	function:
		vector_dot_product() dots two 3D vectors together and returns the value
	*/
	return (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2]);
}

void vector_cross_product(double *v1, double *v2, double *result){
	result[0] = v1[1]*v2[2]-v2[1]*v2[2];
	result[1] = v1[0]*v2[2]-v2[0]*v2[2];
	result[2] = v1[0]*v2[1]-v2[0]*v2[1];
}

double vector_length(double *vector){
	/*
	inputs:
		double *vector: 3D vector
	output:
		double: length of vector
	function:
		vector_length() takes in a 3D vector and returns the length of the vector
		which is the square root of the sum of the squared vector components.
	*/
	return sqrt(pow(vector[0], 2)+pow(vector[1], 2)+pow(vector[2], 2));
}

void vector_reflection(double *N, double *L, double *result){
	/*
	inputs:
		double *N: Normal vector of object
		double *L: Light vector to object
		double *result: the vector to store the result
	output:
		void
	function:
		vector_reflection() gets the reflection of the light vector
		along the normal vector of the surface it is hitting by using
		the equation:
		R=(2N.L)*N-L
	*/
	double dot_result;
	double temp_vector[3]; 
	dot_result = 2*vector_dot_product(N, L);
	vector_scale(N, dot_result, temp_vector);
	vector_subtraction(L, temp_vector, result);

}

void vector_subtraction(double *v1, double *v2, double *result){
	/*
	inputs:
		double *v1: 3D vector origin
		double *v2: 3D vector destination
		double *result: 3D vector to store the resulting vector
	output:
		void
	function:
		vector_subtraction() subtracts v2 from v1 and stores it in result
	*/
	result[0] = v2[0] - v1[0];
	result[1] = v2[1] - v1[1];
	result[2] = v2[2] - v1[2];
}

void vector_addition(double *v1, double *v2, double *result){
	/*
	inputs:
		double *v1: 3D vector origin
		double *v2: 3D vector destination
		double *result: 3D vector to store the resulting vector
	output:
		void
	function:
		vector_addition() adds v2 to v1 and stores it in result
	*/
	result[0] = v2[0] + v1[0];
	result[1] = v2[1] + v1[1];
	result[2] = v2[2] + v1[2];
}


void vector_scale(double *vector, double scalar, double *result){
	/*
	inputs:
		double *vector: 3D vector
		double scalar: value to scale vector by
		double *result: 3D vector to store the result
	output:
		void
	function:
		vector_scale() multiplies a vector by a scalar
	*/
	result[0] = vector[0]*scalar;
	result[1] = vector[1]*scalar;
	result[2] = vector[2]*scalar;
}

//--------------INTERSECTION FUNCTIONS----------------------

double sphere_intersection(double *Ro, double *Rd, double *C, double r) {
	/*
	inputs: 
		double *Ro: origin point of ray (light or camera)
		double *Rd: direction the light/camera is pointing
		double *C: the center of the sphere
		doube r: the radius of the sphere
	output:
		double: the length at which the ray intersects the sphere
	function:
		sphere_intersection() takes a ray (light or camera) and 
		a sphere, and calculates the distance to the intersection of
		the sphere. If the ray intersects the sphere at two points, 
		returns the closest of the two. If no intersection occurs, a
		value of -1 is returned. The following process is used:
		Step 1. Find the equation for the object you are
	  interested in..  (e.g., sphere)
	  
	  
	  x^2 + y^2 + z^2 = r^2
	  Step 2. Parameterize the equation with a center point
	  if needed
	  
	  (x-Cx)^2 + (y-Cy)^2 + (z-Cz)^2 = r^2
	  
	  Step 3. Substitute the eq for a ray into our object
	  equation.
	  
	  (Rox + t*Rdx - Cx)^2 + (Roy + t*Rdy - Cy)^2 + (Roz + t*Rdz - Cz)^2 - r^2 = 0
	  
	  Step 4. Solve for t.
	  
	  Step 4a. Rewrite the equation (flatten).
	  
	  -r^2 +
	  t^2 * Rdx^2 +
	  t^2 * Rdy^2 +
	  t^2 * Rdz^2 +
	  2*t * Rox * Rdx -
	  2*t * Rdx * Cx +
	  2*t * Roy * Rdy -
	  2*t * Rdy * Cy +
	  2*t * Roz * Rdz -
	  2*t * Rdz * Cz +
	  Rox^2 -
	  2*Rox*Cx +
	  Cx^2 +
	  Roy^2 -
	  2*Roy*Cy +
	  Cy^2 +
	  Roz^2 -
	  2*Roz*Cz +
	  Cz^2 = 0
	  
	  Steb 4b. Rewrite the equation in terms of t.
	  
	  t^2 * (Rdx^2 + Rdy^2 + Rdz^2) +
	  t * (2 * (Rox * Rdx - Rdx * Cx + Roy * Rdy - Rdy *Cy Roz * Rdz - Rdz * Cz)) +
	  Rox^2 - 2*Rox*Cx + Cx^2 + Roy^2 - 2*Roy*Cy + Cy^2  + Roz^2 - 2*Roz*Cz + Cz^2 - r^2 = 0
	  
	  Use the quadratic equation to solve for t..
  */
  double a = (pow(Rd[0], 2) + pow(Rd[1], 2) + pow(Rd[2], 2));
  double b = (2 * (Ro[0] * Rd[0] - Rd[0] * C[0] + Ro[1] * Rd[1] - Rd[1] * C[1] + Ro[2] * Rd[2] - Rd[2] * C[2]));
  double c = pow(Ro[0], 2) - 2*Ro[0]*C[0] + pow(C[0], 2) + pow(Ro[1], 2) - 2*Ro[1]*C[1] + pow(C[1], 2) + pow(Ro[2], 2) - 2*Ro[2]*C[2] + pow(C[2], 2) - pow(r, 2);

  double det = pow(b, 2) - 4 * a * c;
  if (det < 0) return -1;

  det = sqrt(det);

  double t0 = (-b - det) / (2*a);
  if (t0 > 0.00001) return t0;

  double t1 = (-b + det) / (2*a);
  if (t1 > 0.00001) return t1;

  return -1;
}

double plane_intersection(double *Ro, double *Rd, double *P, double *N) {
	/*
	inputs:
		double *Ro: origin point of ray (light or camera)
		double *Rd: direction the light/camera is pointing
		double *P: the position of the plane
		doube *N: the normal vector of the plane
	output:
		double: the length at which the ray intersects the plane 
	function:
		plane_intersection takes a ray from a camera or light and 
		a plane, and returns the point at which the ray intersects
		the plane. If an intersection does not occur, returns -1.
		The following steps are used:
  	Step 1. Find the equation for the object you are
    interested in..  (e.g., plNW)
  
    Ax + By + Cz + D = 0 
  
    Step 2. Parameterize the equation with a center point
    if needed
  
    Since N = [A, B, C]
  
    Nx(x-Px) + Ny(y-Py) + Nz(z-Pz) = 0
  
    Step 3. Substitute the eq for a ray into our object
    equation.
  
    Nx(Rox + t*Rdx - Px) + Ny(Roy + t*Rdy - Py) + Nz(Roz + t*Rdz - Pz) = 0
  
    Step 4. Solve for t.
  
    Step 4a. Rewrite the equation (flatten).
  
    NxRox + t*Nx*Rdx - NxPx + NyRoy + t*Ny*Rdy - NyPy + NzRoz + t*Nz*Rdz - NzPz = 0
    
    Steb 4b. Rewrite the equation in terms of t.
  
    t(Nx*Rdx + Ny*Rdy + Nz*Rdz) = Nx*Px + Ny*Py + Nz*Pz - Nx*Rox - Ny*Roy - Nz*Roz
  
    t = (NxPx + NyPy + NzPz - NxRox - NyRoy - NzRoz)/(Nx*Rdx + Ny*Rdy + Nz*Rdz) 
  */
  //normalize the normal vector
  vector_normalize(N);
  double t = (N[0]*P[0] + N[1]*P[1] + N[2]*P[2] - N[0]*Ro[0] - N[1]*Ro[1] - N[2]*Ro[2])/(N[0]*Rd[0] + N[1]*Rd[1] + N[2]*Rd[2]); 
  if (t > 0) return t;

  return -1;
}

//--------------LIGHT FUNCTIONS----------------------

double calculate_diffuse(double object_diff_color, double light_color, double *N, double *L){
	/*
	inputs:
		double object_diff_color: the value of one component of the diffuse color of the object (usually R, G, or B)
		double light_color: the value of one component of the light color (usually R, G or B)
		double *N: the normal vector of the surface
		double *L: the light vector to the surface
	output:
		double: the color value of the diffuse light
	function:
		calculate_diffuse() returns the diffuse value of the light on the surface using the equation:
		Kd*Il(N.L) if N.L>0
		0 if N.L   otherwise
	*/
	double dot_result; 
	dot_result = vector_dot_product(N, L);
	if(dot_result > 0){
		return object_diff_color*light_color*dot_result;
	}
	else{
		return 0;
	}
}

double calculate_specular(double *L, double *N, double *R, double *V, double object_spec_color, double light_color){
	/*
	inputs:
		double *L: the vector of the light to the surface
		double *N: the normal vector of the surface
		double *R: the vector of the reflection of the light from the surface
		double *V: the vector of the camera to the surface
		double object_spec_color: the value of one component of the specular color of the object(usually R, G, or B)
		double light_color: the value of one component of the light color (usually R, G or B)
	output:
		double: the color value of the specular light
	function:
		calculate_specular() calculates the specular value of the light on the surface using the equation:
		Ks*Is(V.R)^ns   if V.R > 0 and N.L > 0
		0								otherwise
		ns is hard-coded to 20
	*/
	double V_dot_R;
	double N_dot_L;
	V_dot_R = vector_dot_product(V, R);
	N_dot_L = vector_dot_product(N, L);
	if(V_dot_R > 0 && N_dot_L > 0){
		return object_spec_color * light_color * pow(V_dot_R, 20);
	}
	else{
		return 0;
	}
}

double frad(Light * light, double t){
	/*
	inputs: 
		Light *light: the light object being used
		double t: the distance to light
	output:
		double: the value the light should be scaled based on distance from light
	function:
		frad() calculates the intensity of the light on the surface according to
		the equation:
	  ______1_______
	  a2*t^2+a1*t+a0
	*/
	return (1/(light->radial_a2*t*t+light->radial_a1*t+light->radial_a0));
}

double fang(Light *light, double *L){
	/*
	inputs:
		Light *light: the light object 
		double *L: the vector of the light to the surface
	output:
		double: the intensity of the light on the surface
	function:
		fang() calculates the intensity of a spotlight on the
		surface.
		If the light is not a spotlight, 1 is returned (full intensity)
		If the object is outside the spotlight, 0 is returned (no intensity)
		Otherwise, the equation used is:
		(VLight.Vobj)^a0
	*/
	double light_length = vector_length(light->direction);
	double theta = light->theta;
	double dot_result;
	double light_vector[3];

	theta = theta*M_PI/180; //convert degrees to radians
	theta = cos(theta); 
	
	light_vector[0] = light->direction[0];
	light_vector[1] = light->direction[1];
	light_vector[2] = light->direction[2];
	
	vector_normalize(light_vector);
	
	if(light->theta == 0 || light_length == 0){
		return 1;
	}
	else{	
		dot_result = vector_dot_product(light_vector, L);
		if (dot_result > theta){
			return 0;
		}
		else{
			return pow(dot_result, light->angular_a0);
		}
	}
	return 0;
}

Closest* shoot(double* Ro, double* Rd, Object** objects){
	Closest* best_values = malloc(sizeof(Closest));
	best_values->closest_object = NULL;
	best_values->closest_t = INFINITY;
	vector_normalize(Rd);
  	for (int i=0; objects[i] != 0; i += 1) {
		double t = 0;
		switch(objects[i]->type) {
			case 0:
		  		t = sphere_intersection(Ro, Rd, objects[i]->position, objects[i]->sphere.radius);
		  		break;
			case 1:
		  		t = plane_intersection(Ro, Rd, objects[i]->position, objects[i]->plane.normal);
		  		break;
			default:
		  		printf("Error: Unknown object type. Element: %d\n", i);	
		  		exit(1);
		}
		if (t > 0.00001 && t < best_values->closest_t) {
			best_values->closest_t = t;
	  		best_values->closest_object = objects[i];
		}
	}
	return best_values;
}

//--------------IMAGE FUNCTIONS----------------------

void generate_scene(Camera *camera, Object **objects, Light **lights, Pixel *buffer, int width, int height){
	/*
	inputs:
		Camera *camera: the camera object which acts as the viewpoint to the scene
		Object **objects: the array of objects to render in the scene
		Light **lights: the array of lights to illuminate the scene
		Pixel *buffer: the array of pixels to be used in writing the image
		int width: the width for the final image
		int height: the height of the final image
	output:
		void
	function:
		generates_scene() uses the camera, objects, and lights given to generate the scene, then
		writes the scene to the pixel buffer.
	*/
  
  double camera_width = camera->width;
  double camera_height = camera->height;
  double pixheight = camera_height / height;
  double pixwidth = camera_width / width;
  Pixel* current_pixel;
  int position;
  for (int y = 0; y < height; y += 1) {
    for (int x = 0; x < width; x += 1) {
      	double Ro[3] = {0, 0, 0};
      	// Rd = normalize(P - Ro)
      	double Rd[3] = {
        	0 - (camera_width/2) + pixwidth * (x + 0.5),
        	0 - (camera_height/2) + pixheight * (y + 0.5),
        	1
  		};
      	vector_normalize(Rd);
  		Closest* nearest_object = shoot(Ro, Rd, objects);
		if (nearest_object->closest_t > 0 && nearest_object->closest_t != INFINITY) {
			current_pixel = recursive_shade(objects, lights, Ro, Rd, nearest_object, 0);
		}	 
		else {
		  	current_pixel->r = 0;
		  	current_pixel->g = 0;
		  	current_pixel->b = 0;
		}
      	position = (height-(y+1))*width+x;
      	buffer[position].r = current_pixel->r;
      	buffer[position].g = current_pixel->g;
      	buffer[position].b = current_pixel->b;
      	free(current_pixel);
    }
  } 
}

void write_p3(Pixel *buffer, FILE *output_file, int width, int height, int max_color){
	/*
	input:	
		Pixel *buffer: the buffer of pixels
		FILE *output_file: the PPM file to write the image
		int width: the width of the image
		int height: the height of the image
		int max_color: the maximum color values allowed
	output: 
		void
	function:
		write_p3() generates a PPM image file in P3 format
	*/
  fprintf(output_file, "P3\n%d %d\n%d\n", width, height, max_color);
  int current_width = 1;
  for(int i = 0; i < width*height; i++){
    fprintf(output_file, "%d %d %d ", buffer[i].r, buffer[i].g, buffer[i].b);
    if(current_width >= 70%12){ //ppm line length = 70, max characters to pixels = 12.
      fprintf(output_file, "\n");
      current_width = 1;
    }
    else{
      current_width++;
    }
  }
}

double clamp(double value){
	/*
	inputs: 
		double value: value to be clamped (usually color values)
	output: 
		double: value betwee 0 and 1 (inclusive)
	function: 
		clamp() returns the value given, restricted to the range of 0 and 1
	*/
	if (value < 0){
		return 0;
	}
	else if (value > 1){
		return 1;
	}
	else{
		return value;
	}
}

Pixel* recursive_shade(Object **objects, Light **lights, double* Ro, double* Rd, Closest* current_object, int depth){
	Pixel* current_pixel = malloc(sizeof(Pixel));
	Object* closest_object = current_object->closest_object;
	double closest_t = current_object->closest_t;
	double color[3];
	double Ron[3];
	double Rdn[3];
	// N, L, R, V
	double N[3];
	double L[3];
	double R[3];
	double V[3];
	double radial_light;
	double angular_light;
	color[0] = 0; // ambient_color[0];
  	color[1] = 0; // ambient_color[1];
  	color[2] = 0; // ambient_color[2];
  	//if reflective, recursively call to get reflection
  	Pixel* reflect = malloc(sizeof(Pixel));
  	reflect->r = 0;
  	reflect->g = 0;
  	reflect->b = 0;
  	Pixel* refract = malloc(sizeof(Pixel));
  	refract->r = 0;
  	refract->g = 0;
  	refract->b = 0;
  		
  	if(current_object->closest_object->reflectivity > 0.00001 && depth <= MAX_DEPTH){ //if it's not reflective, we don't need to calculate this
  		//get angle of reflection from camera
  		double new_ray[3];
  		vector_scale(Rd, -1, new_ray);
  		Ron[0] = closest_t * Rd[0] + Ro[0];
      	Ron[1] = closest_t * Rd[1] + Ro[1];
      	Ron[2] = closest_t * Rd[2] + Ro[2];
      	if(closest_object->type == 1){
			N[0] = closest_object->plane.normal[0]; // plane	W
			N[1] = closest_object->plane.normal[1];
			N[2] = closest_object->plane.normal[2];
		}
		else if(closest_object->type == 0){
			N[0] = Ron[0] - closest_object->position[0]; // sphere
			N[1] = Ron[1] - closest_object->position[1];
			N[2] = Ron[2] - closest_object->position[2];
		}
		else{
			printf("Error: Unknown object type.\n");	
        	exit(1);
		}
		vector_normalize(N);
		vector_normalize(new_ray);
		vector_reflection(N, new_ray, R);
		vector_normalize(R);
		Closest* next_surface = shoot(Ron, R, objects);	
		if(next_surface->closest_t > 0 && next_surface->closest_t < INFINITY){
			//printf("Current object: %d, Next object: %d, distance: %f, reflective depth: %d\n", closest_object->type, next_surface->closest_object->type, next_surface->closest_t, reflect_depth);
			int new_depth = depth+1;
  			reflect = recursive_shade(objects, lights, Ron, R, next_surface, new_depth);
		}
  	}

  	if(closest_object->refractivity > 0.00001 && depth <= MAX_DEPTH){ //if it's not reflective, we don't need to calculate this
  		double new_origin[3];
  		new_origin[0] = closest_t * Rd[0] + Ro[0];
      	new_origin[1] = closest_t * Rd[1] + Ro[1];
      	new_origin[2] = closest_t * Rd[2] + Ro[2];
  		//get angle of refraction from camera
  		double angle_of_intersection;
  		double max_angle = cos(90*M_PI/180);
  		double new_ray[3] = {Rd[0], Rd[1], Rd[2]};
  		/*
  		double a[3];
  		double b[3];
  		double sin_theta;
  		double sin_phi;
  		double cos_phi;
  		double ior;
  		if(closest_object->type == 1){
			N[0] = closest_object->plane.normal[0]; // plane	W
			N[1] = closest_object->plane.normal[1];
			N[2] = closest_object->plane.normal[2];
		}
		else if(closest_object->type == 0){
			N[0] = new_origin[0] - closest_object->position[0]; // sphere
			N[1] = new_origin[1] - closest_object->position[1];
			N[2] = new_origin[2] - closest_object->position[2];
		}
		else{
			printf("Error: Unknown object type.\n");	
        	exit(1);
		}
		
		angle_of_intersection = vector_dot_product(N, Rd);
		printf("A_O_I: %f, max_angle: %f\n", angle_of_intersection, max_angle);
		//IF angle between N.Rd < cos(theta*M_PI/180)
		if(closest_object->type == 0 && angle_of_intersection < max_angle){
			printf("Exiting sphere...\n");
			ior = 1/closest_object->ior;
		}
		else{
			printf("Entering sphere...\n");
			ior = closest_object->ior;
		}
		
		ior = closest_object->ior;
  		vector_cross_product(N, Rd, a);
  		vector_normalize(a);
  		vector_cross_product(N, a, b);
  		sin_theta = vector_dot_product(Rd, b);
  		sin_phi = ior*sin_theta;
  		int value = 1-pow(sin_phi, 2);
  		if (value > 0){
  			cos_phi = sqrt(value);	
  		}
  		else{
  			cos_phi = 0;
  		}
  		
  		vector_scale(N, -1*cos_phi, N);
  		vector_scale(b, sin_phi, b);
  		vector_addition(N, b, new_ray);
  		*/
  		Closest* next_surface = shoot(new_origin, new_ray, objects);
		if(next_surface->closest_t > 0 && next_surface->closest_t < INFINITY){
			int new_depth = depth + 1;
  			refract = recursive_shade(objects, lights, new_origin, new_ray, next_surface, new_depth);
		}
  	}
	for (int j=0; lights[j] != NULL; j+=1) {
    	// Shadow test
  		Ron[0] = closest_t * Rd[0] + Ro[0];
      	Ron[1] = closest_t * Rd[1] + Ro[1];
      	Ron[2] = closest_t * Rd[2] + Ro[2];
      	Rdn[0] = lights[j]->position[0] - Ron[0];
      	Rdn[1] = lights[j]->position[1] - Ron[1];
      	Rdn[2] = lights[j]->position[2] - Ron[2];
      	double distance_to_light = vector_length(Rdn);
      	vector_normalize(Rdn);
      	double shadow_t;
      	int closest_shadow_object = 0;
		//Get N
		if(closest_object->type == 1){
			N[0] = closest_object->plane.normal[0]; // plane	
			N[1] = closest_object->plane.normal[1];
			N[2] = closest_object->plane.normal[2];
		}
		else if(closest_object->type == 0){
			N[0] = Ron[0] - closest_object->position[0]; // sphere
			N[1] = Ron[1] - closest_object->position[1];
			N[2] = Ron[2] - closest_object->position[2];
		}
		else{
			printf("Error: Unknown object type.\n");	
        	exit(1);
		}
		vector_normalize(N);

      	for (int k=0; objects[k] != NULL; k+=1) {
			if (objects[k] == closest_object) {
				continue;
			} 
			switch(objects[k]->type) {
				case 0:
				  	shadow_t = sphere_intersection(Ron, Rdn, objects[k]->position, objects[k]->sphere.radius);
				  	break;
				case 1:
	        		shadow_t = plane_intersection(Ron, Rdn, objects[k]->position, objects[k]->plane.normal);
					break;
				default:
				  	printf("Error: Unknown object type. Element: %d\n", k);	
	        		exit(1);
			}
			if (0 < shadow_t && shadow_t < distance_to_light) {
				closest_shadow_object = 1;
				break;
			}
      	}
      	if (closest_shadow_object == 0) {
			//Get N
			if(closest_object->type == 1){
				N[0] = closest_object->plane.normal[0]; // plane	
				N[1] = closest_object->plane.normal[1];
				N[2] = closest_object->plane.normal[2];
			}
			else if(closest_object->type == 0){
				N[0] = Ron[0] - closest_object->position[0]; // sphere
				N[1] = Ron[1] - closest_object->position[1];
				N[2] = Ron[2] - closest_object->position[2];
			}
			else{
				printf("Error: Unknown object type.\n");	
	        	exit(1);
			}
			vector_normalize(N);
			//Get L
			L[0] = Rdn[0]; // light_position - Ron;
			L[1] = Rdn[1];
			L[2] = Rdn[2];
			vector_normalize(L);
			//Get R
			vector_reflection(N, L, R);
			vector_normalize(R);
			//Get V
			V[0] = -1*Rd[0];
			V[1] = -1*Rd[1];
			V[2] = -1*Rd[2];
			vector_normalize(V);
		 	double diffuse[3];
			diffuse[0] = calculate_diffuse(closest_object->diffuse_color[0], lights[j]->color[0], N, L);
			diffuse[1] = calculate_diffuse(closest_object->diffuse_color[1], lights[j]->color[1], N, L);
			diffuse[2] = calculate_diffuse(closest_object->diffuse_color[2], lights[j]->color[2], N, L);
			double specular[3];
			specular[0] = calculate_specular(L, N, R, V, closest_object->specular_color[0], lights[j]->color[0]);
			specular[1] = calculate_specular(L, N, R, V, closest_object->specular_color[1], lights[j]->color[1]);
			specular[2] = calculate_specular(L, N, R, V, closest_object->specular_color[2], lights[j]->color[2]);
			radial_light = frad(lights[j], distance_to_light);
			angular_light = fang(lights[j], L);
			double reflective[3];
			reflective[0] = ((double)reflect->r)/255;
			reflective[1] = ((double)reflect->g)/255;
			reflective[2] = ((double)reflect->b)/255;
			double refractive[3];
			refractive[0] = ((double)refract->r)/255;
			refractive[1] = ((double)refract->g)/255;
			refractive[2] = ((double)refract->b)/255;
			color[0] += (radial_light * angular_light * (diffuse[0] + specular[0]))*(1-closest_object->reflectivity-closest_object->refractivity);
			color[0] += (closest_object->reflectivity*reflective[0]);
			color[0] += (closest_object->refractivity*refractive[0]);
			color[1] += (radial_light * angular_light * (diffuse[1] + specular[1]))*(1-closest_object->reflectivity-closest_object->refractivity);
			color[1] += (closest_object->reflectivity*reflective[1]);
			color[1] += (closest_object->refractivity*refractive[1]);
			color[2] += (radial_light * angular_light * (diffuse[2] + specular[2]))*(1-closest_object->reflectivity-closest_object->refractivity);
			color[2] += (closest_object->reflectivity*reflective[2]);
			color[2] += (closest_object->refractivity*refractive[2]);
      	}
    }
	current_pixel->r = (unsigned char)(255 * clamp(color[0]));
	current_pixel->g = (unsigned char)(255 * clamp(color[1]));
	current_pixel->b = (unsigned char)(255 * clamp(color[2]));
	return current_pixel;
	
}
