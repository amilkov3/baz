
# Project README file

This is **YOUR** Readme file.

## Project Description

Please see [Readme.md](Readme.md) for the rubric we use for evaluating your submission.

We will manually review your file looking for:

- A summary description of your project design.  If you wish to use grapics, please simply use a URL to point to a JPG or PNG file that we can review

- Any additional observations that you have about what you've done. Examples:
	- __What created problems for you?__
	- __What tests would you have added to the test suite?__
	- __If you were going to do this project again, how would you improve it?__
	- __If you didn't think something was clear in the documentation, what would you write instead?__

## Known Bugs/Issues/Limitations

__Please tell us what you know doesn't work in this submission__

## References

__Please include references to any external materials that you used in your project__

# BEGIN

## Known Bugs/Issues/Limitations

* All the requirements, flow descriptions, and instructions are scattered all over the place and many contradict each other or are vague.  A couple examples: 
	* the param `std::map<std::string,int>* file_map` in `DFSClientNodeP1::List` and the comment in the method don't say what `int` is actually supposed to be. I stumbled on the fact that it should be an epoch time in the `part1.md` README, when if anything this should be either in the method comment or in `part1-sequence.pdf` (but not in both). 
	* `part1-sequence.pdf` defines a file acknowledgement as containing at least the file name and `stat.st_mtime` but the README says it should contain creation time as well. The PDF also asserts that every method should return a file acknowledgement but there's no real mention of this in the README or code comments

	Putting all of these in one place would be best but if this isn't possible then dividing this info into say no more than 2 sections and placing all of the info relevant to that section in that section alone would be the most clear.

*  I was reminded of something I forgot to mention for project 1 and 3, is there any reason why the shared student header files were not paired with source files? In hindsight with all the C/C++ stuff I've learned, I think I could've used `inline` but the standard `extern` in the header and definition in the source wasn't possible if you then `include`d the header in multiple source files since the compiler will complain that you have duplicate declarations. An example of a method I would've wanted to include in multiple source files, both client and server to be specific in the context of project 1 and 3, is a `gfstatus_t_to_str` that returns the string representation of the provided `gfstatus_t` for logging purposes
* Running `make` instructs that `make part1_clean` and `make part2_clean` clean each part when in the make file the commands are actually `clean_part1` and `clean_part2` 
* `part1-sequence.pdf` and `part2-sequence.pdf` claim all methods should have a deadline but for essentially single op methods like `Delete` I feel like that only makes sense if the requests are being buffered prior to actually being processed for example, as in the case of gRPC async. Otherwise the request happens so fast that a deadline is probably unnecessary
* `Delete` comment instructions in `dfslib-clientnode-p1.cpp` are the comment instructions for part 2 `Delete`
* `CallbackList` is confusing. The aliases `FileRequestType` and `FileListResponseType` are named such that it implies it takes a file request and returns a list of file statuses. This is confirmed by the `request.set_name("")` but this doesn't make much sense. Does that mean because the file name is empty simply return all files? gRPC convention for empty requests is something like:
```
message Empty {

}
```

## Assumptions
* I assume we overwrite the entire client file if its already there for part1 `Fetch` and entire server file if its already there for `Store`
* If client already has a lock in `part2`, return OK instead of failing
* `CallbackList` in `part2/src/dfslibx-clientnode-p2.h` is async which means attempting to check `context->IsCancelled()` later in my `ListFiles` in `part2/dfslib-servernode-p2.cpp` throws a runtime exception since its not safe to call when using async API. So I just omitted the deadline check entirely for `ListFiles`

## Design

### Part 1

This was straightforward enough except for the ambiguity across the project resources about what the difference between file acknowledgements and statuses is, if any. For documentation purposes for myself, I opted to define seperate `FileAck` and `FileStatus` proto messages in both parts, where an ack is the same as a status minus creation time and file size fields. I never ended up consolidating them into one although I had `Files` in `part2` contain repeated `FileStatus`es instead of repeated `FileAck`s like in `part1`. I used the gRPC client metadata map to transmit the file name when `Store`ing, instead of passing it along in each `FileChunk` message in the gRPC stream for example. Naturally `Fetch` is also streamed. All requests have deadline timeouts

### Part 2

I started by copying and pasting the code from `part1` into `part2` client and server methods. From there I had to implement server side synchronization which I opted to do at a likely unnecessarily granular level of concurrency. Here's the mutexes and shared data structures I used:

```c++
// Stores which client id has a write lock on which file name
shared_timed_mutex fileNameToClientIdRW;
map<FileName, ClientId> fileNameToClientId;

// Stores the mutex that actually syncronizes reading/writing to/fro the given file
shared_timed_mutex fileNameToRWMutexRW;
map<FileName, unique_ptr<shared_timed_mutex>> fileNameToRWMutex;

// Read/write synchronization to the entire mount directory. Created for ListFiles
shared_timed_mutex dirMutex;
shared_timed_mutex fileNameToClientIdRW;
```

* `fileNameToClientIdRW` - read/write mutex guarding `fileNameToClientId`
* `fileNameToClientId` - an entry is added in the `AcquireWriteLock` method if there isn't already one in which case we return `RESOURCE_EXHAUSTED` if another client id already has a write lock on the given file or `OK` and do nothing if its the same client id. The entry is deleted in the subsequent write call the client makes: either `DeleteFile` or `WriteFile`, regardless of whether it succeeds or fails
*  `fileNameToRWMutexRW` - read/write mutex guarding `fileNameToRWMutex`
* `fileNameToRWMutex` - Facilitates file level locking. First, the mutex for the given file is retrieved if it exists. if it doesn't its created then and there. all server methods either `lock_shared()` said mutex for reads or exclusively `lock()` it for writes. For simplicity and to reduce writes (subsequently contention), we do not remove entries in the map when a file is deleted
* `dirMutex` - Facilitates mount directory level locking. Everywhere we lock/unlock a file level mutex we also lock/unlock this mutex. It was motivated originally to simplify `ListFiles`, so we don't need to read each file level mutex out of `fileNameToRWMutex`, acquire it, and release it as we loop through the members of the mount directory for example, which wouldn't be a snapshot in time of the mount directory anyway. At this point I realized I could've just use this directory level mutex instead of the `fileNameToRWMutex` ceremony in the first place. But as was discussed in a concurrency paper towards the beginning of the semester, synchronization granularity is simply a tradeoff between performance and code complexity/likelihood of race conditions, so I kept it.

There's numerous additional functionality that needed to be added to the part2 server methods. In `Fetch` and `Store`, the client file's checksum is passed in gRPC client metadata and compared against the same file's checksum on the server, if it already exists. If they're the same, no transmission of file contents needs to occur and an `ALREADY_EXISTS` can be returned, but the file's modified time (`mtime`) needs to be updated. If the server file's `mtime` is newer than the client's during a `Fetch`, the client file's is set to the server file's. And vice versa during a `Store`. The `mtime` is also conveyed in client metadata along with the client id when acquiring a write lock on the server and asserting that the current write lock's client id on the given file and the client's client id are the same during a write. As in `part1` the file name is also placed in client metadata.
The biggest difference between `part1` and `part2` is the client side async and watcher threads and the addition of a `CallbackList` method server side. Essentially, the async thread continuously issues calls to `CallbackList` which is a front for `ListFiles` which gives the client a list of server files and the client fetches and stores what it needs to synchronize the contents of its mount dir with the server's one to one. In the other direction, the watcher thread monitors the client's mount dir and when a file is modified or deleted an `inotify` event is emitted which triggers the client to issue a store or delete to the server so the server can sync its mount dir with this specific client's mount dir, one to one. So in order to coordinate this and avoid race conditions, we need client side synchronization now too. Because I waited until the end to finish the last part of the assignment, for the sake of simplicity I decided to just use one regular (not read/write) `mutex` client side. Since both the async and writer threads are write paths anyway, thats why using a read/write mutex doesn't buy us anything. So when either of these threads issue their calls to sync with the server, we just lock the mutex and unlock it when they're finished

## Feedback

This assignment seemed a bit much for an intro class. And there's nothing wrong with such a challenge (within reason) but considering how much I struggled as a professional engineer, I can't imagine what it was like for classmates who aren't, and I couldn't help thinking it would've went a lot smoother in Golang for example, but I suppose the step up to C++ from C is more...natural and perhaps just as academically suited to the class as C. The truth is though assuming no prior experience in either, the step from C to C++ vs C to Golang is more involved, even for a project like this that didn't necessarily require a deep dive into C++ . For example, not having a `defer` construct like Golang or even a `try/finally` like most languages made it really easy to miss releasing mutexes, particularly if you opted for a granular synchronization level for this project. As far as I understand, you're supposed to use destructors to achieve something similar, but I opted not to take the time out to learn how to use them since I spent most of the project wrangling C++ and after a point I just wanted to finish the project and get most of the test cases to pass. In any case, the point of the assignment is to learn and gain experience with RPCs and distributed file systems, which Golang would balance out the fact that this final project is by far the most involved thus far, as it gets out of the programmer's way as much possible. Wrestling with C++ on the other hand was frustrating for a good chunk of the time and it felt mostly unnecessary since the OS APIs that needed to be used were minimal and nothing we hadn't worked with in C already. And since most of the time was spent Googling for gRPC C++ documentations and code snippets, there's significantly more such materials in Golang. Besides that, judging by Piazza students struggled to understand the mechanism of the asynca and watcher threads in `part2` and I was no exception. Further elaboration and clarity around this in the project resources would help alot. It may also lighten the load to include one of the simpler proto service rpc methods (so as not to essentially give away the others) like `DeleteFile` as part of the project skeleton to guide students, since I spent most of my time getting the client and server RPC methods in part 1 and 2 right, rather than the synchronization related stuff, which is perhaps more relevant to the topic of the assignment

In light of students struggling with these issues, thank you for the extension. Many other classes would simply blame the students entirely, or moreso not even bother addressing it at all