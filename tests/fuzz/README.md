# Fuzzing

The C-Blosc2 project is integrated with Google's [OSS-Fuzz](https://github.com/google/oss-fuzz) project to provide automated fuzz testing. Testing is performed continuously on the source tree's default branch and instanteously whenever new pull requests are created.

## Background

A fuzzing engine provides randomized data inputs into a fuzz target or application in order to produce exceptions and crashes.

## Getting Started

There are several fuzz applications already available that can be used as a starting point for any future fuzzing work:

|Source|Description|
|-|-|
|fuzz_compress_frame.c|Frame compression fuzz testing|
|fuzz_decompress_frame.c|Frame decompression fuzz testing|
|fuzz_compress_chunk.c|Blosc chunk compression fuzz testing|
|fuzz_decompress_chunk.c|Blosc chunk decompression fuzz testing|

So long as the source file begins with "fuzz\_" it will be automatically picked up by CMake, built, and used in automated fuzz testing.

The signature of the entry point function for fuzzing that is used by the LLVM fuzzing engine `libFuzzer` is as follows:

```
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
```

The `data` and `size` variables are the buffer with randomized data and the size of that randomized data respectively.

### Standalone Fuzzing

During automated fuzz testing each fuzzer is linked with `libFuzzer` which is able to track which areas of code are reached.

At times it may be necessary to run a fuzzer as a standalone application in order to reproduce a particular failure case. CMake has been setup to automatically create standalone fuzzer projects that can be used for this purpose.

With these projects each fuzzer is compiled with *standalone.c* which is a command line application shim that takes as its command line arguments paths to files that contain the randomized data to feed into the fuzzer.

### Seed Corpus

A seed corpus is a collection of files that can be used as a starting point for the fuzzing engine to begin testing. In order to use a seed corpus it must be located in the output directory after building. Each seed corpus should be a zip file with each zip entry being a seed that the fuzzer can start with. The name of the zip file must end with "\_fuzzer\_seed\_corpus.zip" and the beginning must match the name of the source file after "fuzz\_".

Seeds can additionally be created from existing folders by utilizing the C-Blosc2 project [build script](https://github.com/google/oss-fuzz/blob/master/projects/c-blosc2/build.sh) in OSS-Fuzz.

## Viewing Crashes

All application crashes that are found through automated tests and their associated reproduction data are cataloged on the oss-fuzz.com website and are viewable by only by members specified in c-blosc2's [project configuration](https://github.com/google/oss-fuzz/blob/master/projects/c-blosc2/project.yaml).

Application crashes that are found during creation of pull requests can be reproduced using the information found in the _CI Fuzz_ build log. It will include a Base64 encoding of the data used to reproduce the crash. That can be converted to binary data using a converter and saved to a file. With that it is then possible to use that file along with the standalone fuzzer that reproduced it in order to investigate the crash.

## OSS-Fuzz Integration

C-Blosc2's integration with OSS-Fuzz can be found in the [c-blosc2 project folder](https://github.com/google/oss-fuzz/tree/master/projects/c-blosc2) in the oss-fuzz repository. These configuration files control:

 * Sanitizers are run
 * Architectures tests are performed on
 * Who is notified of automated testing crashes
 * Build scripts for OSS-Fuzz to compile C-Blosc2

## Additional Resources

* [libFuzzer](https://releases.llvm.org/8.0.0/docs/LibFuzzer.html) - a library for coverage-guided fuzz testing
* [c-blosc2](https://github.com/google/oss-fuzz/tree/master/projects/c-blosc2) - project configuration in oss-fuzz
* [oss-fuzz](https://oss-fuzz.com/) - view application crashes and exceptions
