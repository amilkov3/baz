
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

## Issues

* `part1-sequence.pdf` 
	* defines a file acknowledgement as containing at least the file name and `stat.st_mtime` but the README says it should contain creation time as well
	* asserts that every method should return a file acknowledgement but no real mention of this in the README and, though doing so makes sense, it's a little confusing for `Delete` for example
* I forgot to mention for project 1 and 3, is there any reason why the shared header files were not paired with source files? I haven't written C to any extent really before this class so by no means an expert but from all the sources I found online, you can define methods in a header file but you may then only `include` that header file in a single source file otherwise the methods will be linked with every source file the header file is `include`d in and the compiler will complain that you have duplicate declarations. An example of a method I would've wanted to include in multiple source files, both client and server to be specific in the context of project 1 and 3, is a `gfstatus_t_to_str` that returns the string representation of the provided `gfstatus_t` for logging purposes. Another is safe methods of phread methods like:
```c
static void Pthread_mutex_lock(pthread_mutex_t *mutex) {
  int rc = pthread_mutex_lock(mutex);
  assert(rc == 0);
}
```
Neither of these motivating examples are critical to any part of these of assignments obviously, but they are helpful especially to novices and arguably good practice anyway
* All the requirements, flow descriptions, and instructions are scattered all over the place. For example, the param `std::map<std::string,int>* file_map` in `DFSClientNodeP1::List` and the comment in the method don't say what `int` is actually supposed to be. I stumbled on the fact that it should be an epoch time in the `part1.md` README, when if anything this should be either in the method comment or in `part1-sequence.pdf` (but not in both). Putting all of these in one place would be best but if this isn't possible then dividing this info into say no more than 2 sections and placing all of the info relative to that section in that section and no where else would be the most clear.
* Running `make` says `make part1_clean` and `make part2_clean` when in the make file they're actually `clean_part1` and `clean_part2` 
* Why is there a deadline set on `Delete` 
* `Delete` instructions in `dfslib-clientnode-p1.cpp` are the instructions for part 2 `Delete`
* `CallbackList` is confusing. In the code it asserts it takes a file request and returns a list of file statuses. This makes no sense. Particularly `request.set_name("")`. Does that mean because the file name is empty simply return all files? gRPC convention for empty requests is something like:
```
message Empty {

}
```
If you want people to simply pass in a file request with an empty string for name just say so explicitly

## Assumptions
* I assumed `int` in param `std::map<std::string,int>* file_map` in `DFSClientNodeP1::List`is a UNIX timestamp i.e. number of seconds that have elapsed since the Unix epoch
* I assumed that `Store` in part1 should overwrite the file if it exists
* Should we overwrite client file if its already there for part1 fetch?
* If client already has a lock, return OK instead of failing

## Design

## Feedback

This assignment seemed a bit much. I couldn't help thinking it would've went a lot smoother in Golang, for example, but I guess the step up to C++ from C is more...natural? The truth is though assuming no prior experience in either, the step from C to C++ vs C to Golang would be at least as involved, even for a project like this that didn't necessarily require a deep dive into C++ . For example, not having a `defer` construct like Golang or even a `try/finally` like most languages made it really easy to miss releasing mutexes, particularly if you opted for a granular synchronization level for this project. As far as I understand, you're supposed to use destructors to achieve something similar, but I opted not to take the time out to learn how to use them since I spent most of the project wrangling C++ and after a point I just wanted to finish the project and get most of the test cases to pass. In any case, the point of the assignment is to learn and gain experience with RPCs and distributed file systems, which Golang would balance out the fact that this final project is by far the most involved thus far, as it gets out of the programmer's way as much possible. Wrangling C++ on the other hand was frustrating for a good chunk of the time and it felt mostly unnecessary since the OS APIs that needed to be used were minimal and nothing we hadn't worked with in C already. And since most of the time was spent Googling for gRPC C++ documentations and code snippets, there's significantly more such materials in Golang

In light of this assignment's skeleton and instructions/resources not being as ironed out as previous assignments, thank you for the extension. Many other classes would simply place 100% of the blame on students, or moreso not even bother addressing it at all