 GRPC and Distributed Systems

## Forward

In this project, you will design and implement a simple distributed file system (DFS).  First, you will develop several file transfer protocols using gRPC and Protocol Buffers. Next, you will incorporate a weakly consistent synchronization system to manage cache consistency between multiple clients and a single server. The system should be able to handle both binary and text-based files.

Your source code will use a combination of C++14, gRPC, and Protocol Buffers to complete the implementation.

## Setup

You can clone the code in the Project 4 repository with the command:

```
git clone https://github.gatech.edu/gios-fall19/pr4.git
```

## Submission Instructions

Submit all code via the `submit.py` script given at the top level of the repository. For instructions on how to submit individual components of the assignment, see the instructions within [Part 1](docs/part1.md) and [Part 2](docs/part2.md). Instructions for submitting the readme are in the next section.

For this assignment, you may submit your code up to 10 times in 24 hours. After the deadline, we download your last submission before the deadline, review your submission, and assign a grade. There is no limit to the number of times you may submit your `readme-student.md` file.

After submitting, you may double-check the results of your submission by visiting the [Udacity/GT Bonnie website](https://bonnie.udacity.com) and going to the student portal.

## Readme

Throughout the project, we encourage you to keep notes on what you have done, how you have approached each part of the project, and what resources you used in preparing your work. We have provided you with a prototype file, `readme-student.md` that you should use throughout the project.

You may submit your `readme-student.md` file with the command:

```
python submit.py readme
```

At the prompt, please provide your GT username and password.

If this is your first time submitting, the program will then ask you if you want to save the JWT. If you reply yes, it will save a token on your filesystem so that you don't have to provide your username and password each time that you submit.

The Udacity site will store your `readme-student.md` file in a database, where it will be used during grading. The submit script will acknowledge receipt of your README file. For this project, like in Project 3, you only need to submit one README file for both parts of the project.

> Note: you may choose to submit a PDF version of this file (`readme-student.pdf`) in place of the markdown version. The submission script will automatically detect and submit this file if it is present in the project root directory. If you submit both files, we will give the PDF version preference.

## Directions

- Directions for Part 1 can be found in [docs/part1.md](docs/part1.md)
- Directions for Part 2 can be found in [docs/part2.md](docs/part2.md)

## Log Utility 

We've provided a simple logging utility, `dfs_log`, in this assignment that can be used within your project files. 

There are four log levels (LL_SYSINFO, LL_ERROR, LL_DEBUG, LL_DEBUG2, and LL_DEBUG3). 

During Bonnie tests, only log levels LL_SYSINFO, LL_ERROR, and LL_DEBUG will be output. The others will be ignored. You may use the other log levels for your own debugging and testing. 

The log utility uses a simple streaming syntax. To use it, make the function call with the log level desired, then stream your messages to it. For example: 

```
dfs_log(LL_DEBUG) << "Type your message here: " << add_a_variable << ", add more info, etc."
```
 
## References

### Relevant lecture material

- [P4L1 Remote Procedure Calls](https://www.udacity.com/course/viewer#!/c-ud923/l-3450238825)

### gRPC and Protocol Buffer resources

- [gRPC C++ Reference](https://grpc.github.io/grpc/cpp/index.html)
- [Protocol Buffers 3 Language Guide](https://developers.google.com/protocol-buffers/docs/proto3)
- [gRPC C++ Examples](https://github.com/grpc/grpc/tree/master/examples/cpp)
- [gRPC C++ Tutorial](https://grpc.io/docs/tutorials/basic/cpp/)
- [Protobuffers Scalar types](https://developers.google.com/protocol-buffers/docs/proto3#scalar)
- [gRPC Status Codes](https://github.com/grpc/grpc/blob/master/doc/statuscodes.md)
- [gRPC Deadline](https://grpc.io/blog/deadlines/)

## Rubric

Your project will be graded at least on the following items:

- Interface specification (.proto)
- Service implementation
- gRPC initiation and binding
- Proper handling of deadline timeouts
- Proper clean up of memory and gRPC resources
- Proper communication with the server
- Proper request and management of write locks
- Proper synchronization of files between multiple clients and a single server
- Insightful observations in the Readme file and suggestions for improving the class for future semesters

#### gRPC Implementation (35 points)

Full credit requires: code compiles successfully, does not crash, files fully transmitted, basic safety checks, and proper use of gRPC  - including the ability to get, store, and list files, along with the ability to recognize a timeout. Note that the automated tests will test some of these automatically, but graders may execute additional tests of these requirements.

#### DFS Implementation (55 points)

Full credit requires: code compiles successfully, does not crash, files fully transmitted, basic safety checks, proper use of gRPC, write locks properly handled, cache properly handled, synchronization of sync and inotify threads properly handled, and synchronization of multiple clients to a single server. Note that the automated tests will test some of these automatically, but graders may execute additional tests of these requirements.

#### README (10 points + 5 point extra credit opportunity)

* Clearly demonstrates your understanding of what you did and why - we want to see your design and your explanation of the choices that you made and why you made those choices. (4 points)
* A description of the flow of control for your code; we strongly suggest that you use graphics here, but a thorough textual explanation is sufficient. (2 points)
* A brief explanation of how you implemented and tested your code. (2 points)
* References any external materials that you consulted during your development process (2 points)
* Suggestions on how you would improve the documentation, sample code, testing, or other aspects of the project (up to 5 points extra credit available for noteworthy suggestions here, e.g., actual descriptions of how you would change things, sample code, code for tests, etc.) We do not give extra credit for simply reporting an issue - we're looking for actionable suggestions on how to improve things.

### Questions

For all questions, please use the class Piazza forum or the class Slack channel so that TA's and other students can assist you.

