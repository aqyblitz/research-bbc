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

There are two configuration files that contain information regarding the problem parameters and server parameters. These parameters are introduced to the system as macros at compile-time.

###### problemConfig.json
`blockTotal`: the total number of blocks, and therefore partitions, the input adjacency matrix is split into.

`redMult`: the redundancy multiplier for fault tolerance & a robust system. This is NOT functional yet; setting this value to anything but `1` will break the program.

`vertexTotal`: the number of vertices in the input graph.

`infinityVal`: the value to use for infinity.

`computeParam`: a parameter that defines the behavior of the system.
* `=0`: the "default" parameter; sets the system to solve for the all-pairs shortest-path matrix and output it.
* `=1`: sets the client to generate and keep track of the "next" vertex and the server to output the path from some source vertex to a destination vertex.
* `=2`: sets the server to write a vertex's row of shortest-path data to a file in /data/vertex_row.txt. For use with a Python file that will output the 5 closest/furthest vertices.

`paramA`: a sub-parameter of computeParam. Its use and implications on the system's behavior vary based on `computeParam`'s value.
* `computeParam=0`: N/A
* `computeParam=1`: source vertex for path reconstruction
* `computeParam=2`: source vertex for finding closest/furthest vertices (or row of the shortest-path matrix to write to file)

`paramB`: a sub-parameter of computeParam. Its use and implications on the system's behavior vary based on `computeParam`'s value.
* `computeParam=0`: N/A
* `computeParam=1`: destination vertex for path reconstruction
* `computeParam=2`: number of closest/furthest vertices to output. This parameter doesn't do anything. The original intent was to use a key, value priority queue/heap to make a faster implementation of finding the closest/furthest vertices, but was left out for the current code release.

###### config.json
`socketPort`: the port on the server that is opened for clients to connect to.

`host`: the client connects to the server at this IP address with `socketPort`.

`filePath`: the file to read the input adjacency matrix from.

`debug`: `0` to silence debug messages to stdout. `1` to not do so.

###### Running the code
The JavaScript can be run from the `/run` directory.

Server: `node server.js`

Client: `node client.js`

Computation on the problem does not begin until a sufficient number (i.e. the # of partitions of the adjacency matrix) of clients have connected to the server.

=========
#### Testing

floydwarshall.py is a graph generator and solver. 

##### Parameters
It takes the following command line inputs (in order):

`gen` - 1 to generate a new graph, 0 to use a generated graph with the given file name

`filename` - name of the files to be generated, output files will be filename.in (input file), filename.test (input file for BBC), filename.out (solution), and filename.diff (file to diff with BBC output)

`size` - number of vertices in the graph

`int_only` - 1 for integer only edge weights, 0 for floating point

`num_range` - range of the edge values to be generated, from (0, range value]

`inf_val` - value to be used for infinity, use "inf" (no quotes) for floating point infinity

`edge_prob` - number from [0,1] to determine likelihood of having an edge between two vertices

`undirected` - 1 for an undirected graph, 0 for a directed graph

##### Usage
Run floydwarshall.py to generate a graph. An example run:

`python floydwarshall.py 1 sample 5 1 1 inf .3 1`

This yields four files, sample.in, sample.test, sample.out, and sample.diff:

sample.in
```
0 1 1 inf inf
1 0 inf 1 inf
1 inf 0 inf inf
inf 1 inf 0 inf
inf inf inf inf 0
```

sample.test
```
0 1 1 0 0
1 0 0 1 0
1 0 0 0 0
0 1 0 0 0
0 0 0 0 0
```

sample.out
```
0 1 1 2 inf
1 0 2 1 inf
1 2 0 3 inf
2 1 3 0 inf
inf inf inf inf 0
```

sample.diff
```
0 1 1 2 0
1 0 2 1 0
1 2 0 3 0
2 1 3 0 0
0 0 0 0 0
```

Run the BBC implementation on sample.test by setting it as the `filePath` field in the config.json configuration file. Run diff on the output of BBC and the sample.diff file. If they match the BBC implementation of Floyd-Warshall ran correctly.

##### Marvel Universe Social Graph
The input file is parser/socialgraph.txt. The program parser/parser.py takes this file as a command line argument and produces two files: marvel.in and marvel.test. The marvel.in file is for use with the python program floydwarshall.py. The marvel.test file is for use with the BBC implementation. Once generated, follow the instructions above to run it. The program parser/convert.py takes three command line parameters: the original socialgraph.txt, the row file outputed by the BBC implementation, and k = the number of closest/furtherst characters to output. 
