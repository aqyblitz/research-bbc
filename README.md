research-bbc
=========
A project leveraging BBC to solve the all pairs shortest path problem.

Authors: Andrew Yang, Dasith Gunawardhana

Emscripten is a Mozilla utility that produces JavaScript from LLVM bytecode, which we use here to produce a browser-based parallelized solution to the all-pairs shortest path problem. This repository contains three implementations of the Floyd-Warshall algorithm:

1. simple_serial
  * This is the serial implementation of the algorithm, written to test performance against the parallelized implementations.
2. simple_parallel
  * Using [this paper](http://www.cse.psu.edu/~huv101/files/papers/sbgv_2007_icpads.pdf), we developed a parallel implementation of the FW algorithm following the primary/replica model. 
3. emscript_parallel
  * Following the logic employed in developing the “simple” parallel solution above, we adapted the logistics of the algorithm to fit an asynchronous patter. This was to leverage the asynchronous callback and main loop architecture that Emscripten offers us to produce an effective JavaScript-based solution.

=========
#### simple_serial
C++ code that can be compiled and run as such. 

##### Compilation
`g++ main.cpp`

##### Usage
The code takes a command line parameter that points to a local input file with the graph stored as an adjacency matrix. The input data should have a graph that is unweighted, since we chose not to use the max integer value to simulate infinity and implement arithmetic overflow checks; this is because achieving a working implementation was prioritized. See the sample input data for formatting.

`./a.out <path_to_file>`
`path_to_file` is a path to the input data file.

=========
#### simple_parallel
C++ code that can be compiled and run as such.

##### Compilation
Server: `g++ -o server server.cpp`
Client: `g++ -o client client.cpp`

##### Usage
`./server <num_blocks> <red_mult> <vertex_total>`
The server default to run on port 8888.
`num_blocks` is the total number of blocks/partitions.
`red_mult` is the redundancy multiplier for fault tolerance. Setting this to any value but `1` will break the code.
`vertex_total` is the total number of vertices in the input graph.

This code has not been set up to accept local file input, but has a hard-coded example in the server code, the reason being that this was a proof-of-concept of the algorithm’s implementation.

`./client <host> <port_num>`
`host` and `port_num` form the address of the server to which this replica (client) should connect.

In order for this code to run, an ADDITIONAL client MUST connect to the server, for no purpose other than to receive nonsense output across the socket stream. We could not figure out why this happened.

=========
#### emscript_parallel
This is an asynchronous parallelization of the FW algorithm, using the primary/replica architecture. The source files are located in `<root_dir>/src/`.

The bug requiring an extra & useless client is not present in this implementation.

##### Installation
[Emscripten](http://kripken.github.io/emsripten-site), [nodejs](http://nodejs.org), and [npm](https://www.npmjs.com/) must be installed properly to run this code. Note that node comes with Emscripten.

To obtain the necessary node dependencies: `npm install`
We use Grunt to compile our files; this will install the command line interface for Grunt: `npm install –g grunt-cli`

Run these commands from the root directory. The nodejs output files will be placed into <root_dir>/run/.

##### Compilation
Compiling with the CLI:
* Both: `grunt compile`
* Server (primary) only: `grunt compile-server`
* Client (replica) only: `grunt compile-client`.

Compiling without the CLI:
* Both: `node –e “require(‘grunt’).tasks([‘compile’]);”`
* Server: `node –e “require(‘grunt’).tasks([‘compile-server’]);”`
* Client: `node –e “require(‘grunt’).tasks([‘compile-client’]);”`

##### Usage
Note: The bug requiring an extra client to connect is not present in the Emscript’d code.

There are two configuration files that contain information regarding the problem parameters and server parameters.

###### problemConfig.json
`blockTotal`: the total number of blocks, and therefore partitions, the input adjacency matrix is split into.

`redMult`: the redundancy multiplier for fault tolerance & a robust system. This is NOT functional yet; setting this value to anything but `1` will break the program.

`vertexTotal`: the number of vertices in the input graph.

###### serverConfig.json
`socketPort`: the port on the server that is opened for clients to connect to.

`host`: the client connects to the server at this IP address with `socketPort`.

###### Running the code
The JavaScript can be run from the `/run` directory.

Server: `node server.js`
Client: `node client.js`

Computation on the problem does not begin until a sufficient number (i.e. the # of partitions of the adjacency matrix) of clients have connected to the server.
