PageRank
========

Run ```make``` to generate executables, then use the syntax explained below

The first argument to the executable specifies number of threads, the second one is size of the array.

To run with P number of threads, N element array:
   ```./main P N```

**Notes**

The executable then outputs the time in seconds that the program took to run.

```sum_reduction_active.cc``` updates sum via active routing instead of using a distributed data structure.
