# 3 Color Graph

A console application which creates solutions to the 3 coloring problem of a graph. This project was used to learn 
about process syncronization and inter process communication. Process syncronization was implemented through the use 
of semaphores and inter process communication through shared memory.

## Getting Started

Clone the repository and navigate to the /src folder. Compile the program and run the supervisor. After starting the 
supervisor program run the generator program. The generator will generate 3 coloring solutions to the given graph. If the given graph is 3 colorable the supervisor will terminate after printing the solution to the console and terminating the generators.

Compile the program:
```
$ make all
```
Clean up compiled files:
```
$ make clean
```

Run supervisor:
```
$ ./supervisor
[./supervisor] Solution with 1 edges: 0-2
[./supervisor] The graph is 3-colorable!
```

Run 1 generator:
```
$ ./generator 0-1 0-2 1-2
```
Run 5 generators:
```
$ for i in {1..5}; do (./generator 0-1 0-2 1-2 &); done
```

### Additional Information

The application was only tested on Linux.
