How to look into fuzzer issues
==============================

* Look into the output of the test case in the logs in CI (look for something like `Test unit written to ./crash-85283a6341337b0ede4bf3b9b2657dcce83ee0b4\nBase64: AiACDQEAAAAPAAAAFAAAABMAbZ0=\n".`).  Use the blosc.oss.fuzz@gmail.com account so as to access it.

* This output always gives the base64 of the data that reproduced it (e.g. AiACDQEAAAAPAAAAFAAAABMAbZ0=).  This can be converted to hex and saved to a file.

* This file can be passed as an argument to the corresponding test of `tests/fuzz/` folder, so as to test it locally.
