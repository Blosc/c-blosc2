How to look into fuzzer issues
==============================

* Look into the output of the test case at: https://oss-fuzz.com/testcases?showall=1 (Nathan, I am not sure how to look into a specific testcase coming from the logs in CI; could you chime in and put the procedure here ?).  Use the blosc.oss.fuzz@gmail.com account so as to access it.

* This output always gives the base64 of the data that reproduced it (e.g. AiACDQEAAAAPAAAAFAAAABMAbZ0=).  This can be converted to hex and saved to a file.

* This file can be passed as an argument to `tests/fuzz/decompress_fuzzer` so as to test it locally.
