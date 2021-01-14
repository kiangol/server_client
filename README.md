# server_client

An implementation of flow- and loss control for UDP in C, as part of home exam in the course **IN2140 - OS and datacommunication.** 

## Setup and run
Developed anad tested on macOS Catalina 10.5

### Running

## Normal run without simulated loss
Step 1: Compile
```
make
```

Step 2: Start the server in a terminal window:
```
make checkserver
```

Step 3: Start the client in a second terminal:
```
make check
```

Now the client should start sending over the files to the server and write output log to ```results.txt```

## Run with packet loss
To run the client with a simulated packet loss of 10%:

Step 1: start the server
```
make checkserver
```

Step 2: start the client in loss-mode
```
make checkloss


## Check for memory leaks
Step 1: Run the server with valgrind:
```
make valgrindserver
```

Step 2: Run the client with valgrind:
```
make valgrind
