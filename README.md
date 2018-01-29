# McSimA+
McSimA+ [1] models x86-based asymmetric manycore microarchitectures
for both core and uncore subsystems, including a full spectrum of
asymmetric cores from single-threaded to multithreaded and from in-
order to out-of-order, sophisticated cache hierarchies, coherence
hardware, on-chip interconnects, memory controllers, and main memory.
McSimA+ was once called McSim, when Jung Ho Ahn started to develop
the simulator at HP Labs. Please contact Jung Ho Ahn (gajh@snu.ac.kr)
for technical questions.




## Requirements

McSimA+ utilizes the user-level pthread library by Pan et al. [2],
which is implemented on top of Pin.  McSimA+ requires Pin, a dynamic
instrumentation tool, so download Pin at http://www.pintool.org.
As of now, McSimA+ is developed and tested in the Linux environment.




How to compile
--------------

1. Download Pin at http://www.pintool.org and unzip it. (it was tested with pin 2.14)
2. Download McSimA+ and unzip it.  Now we assume that
- McSimA+ is located at:  `${MCSIM_PARENT_DIR}`
- Pin is located at: `${PIN_HOME}`
3. Compile McSimA+ by

   ```shell
   $ cd ${MCSIM_PARENT_DIR}
   $ make INCS=-I${MCSIM_PARENT_DIR}/pin_kit/extras/xed2-intel64/include -j 8
   ```

-  McSimA+ needs snappy compression library from Google.

   - http://code.google.com/p/snappy/
   - please install and setup proper environmental variables such as

    ```shell
   $ export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
    ```

-  The summary is as follow:

   ```shell
   $ cd ${MCSIM_PARENT_DIR}/McSim
   $ make INCS=-I${PIN_HOME}/extras/xed2-intel64/include -j8
   # comment: you should have snappy installed and it is assumed in /usr/local/lib
   $ export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
   ```
4. Compile Pthread

   ```shell
   $ cd ${MCSIM_PARENT_DIR}/Pthread
   $ make TOOLS_DIR=${MCSIM_PARENT_DIR}/pin_kit/source/tools -j 8
   ```

5. Compile a test program 'stream'

   ```shell
   $ cd ${MCSIM_PARENT_DIR}/McSim/stream
   $ make
   ```

6. **Summary**

   All environtment variables are set in `setup_env` file and compilation variables declared in Makefiles, can simply run:

   ```shell
   $ git clone https://github.tamu.edu/jyhuang/active-routing.git
   $ cd active-routing
   $ source setup_env
   $ cd McSim && make -j8 && cd ..
   $ cd Pthread && make -j8 && cd ..
   ```



How to run
----------

Turn off ASLR.  ASLR makes memory allocators return different virtual address per program run.
- how to turn it off depends on Linux distribution.

   - In RHEL/Ubuntu:
        ```shell
        $ sudo echo 0 > /proc/sys/kernel/randomize_va_space
        ```

   - See below if do not have sudo privilege to execute the command to turn off ASLR
1. Test run

   ```shell
   $ export PIN=${PIN_HOME}/intel64/bin/pinbin
   $ export PINTOOL=${MCSIM_PARENT_DIR}/Pthread/mypthreadtool
   $ export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH	# comment: for snappy
   $ export LD_LIBRARY_PATH=${PIN_HOME}/intel64/lib-ext:$LD_LIBRARY_PATH	# comment: for dwarf
   $ cd ${MCSIM_PARENT_DIR}/McSim
   $ ./mcsim -mdfile ../Apps/md/md-o3-closed.py -runfile ../Apps/list/run-test.py
   ```


2. Test stream

   ```shell
   $ export PATH=$PATH:${MCSIM_PARENT_DIR}/McSim/stream/:
   $ cd ${MCSIM_PARENT_DIR}/McSim
   $ ./mcsim -mdfile ../Apps/md/md-o3-closed.py -runfile ../Apps/list/run-stream.py
   ```


3. When do not have sudo privilege to turn off ASLR for the system
- Turn off ASLR when launching McSimA+ simulation

   ```shell
   $ setarch uname -m -R ./mcsim -mdfile ../Apps/md/md-o3-closed.py -runfile ../Apps/list/run-test.py
   ```

   ​


How to generate traces
----------------------

1. Compile tracegen

   ```shell
   $ cd ${MCSIM_PARENT_DIR}/TraceGen
   $ make TOOLS_DIR=${MCSIM_PARENT_DIR}/pin_kit/source/tools -j 8
   ```


2. Run tracegen

   ```shell
   $ ${MCSIM_PARENT_DIR}/pin_kit/intel64/bin/pinbin -t ${MCSIM_PARENT_DIR}/TraceGen/tracegen -prefix prefix_name -slice_size size point1 point2 ... -- exectuable to extract traces
   ```




## References

[1] J. Ahn, S. Li, S. O and N. P. Jouppi, McSimA+: "A Manycore Simulator with Application-level+ Simulation and Detailed Microarchitecture Modeling", in *Proceedings of the IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS)*, Austin, TX, USA, April 2013.

[2] H. Pan, K. Asanovic, R. Cohn and C. K. Luk, "Controlling Program Execution through Binary Instrumentation", *Computer Architecture News*, vol.33, no.5, 2005.



