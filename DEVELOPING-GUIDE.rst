Some conventions used in C-Blosc2
=================================

* Use C99 designated initialization only in examples. Libraries should use C89 initialization, which is more portable, specially with C++ (designated initialization in C++ is supported only since C++20).

* Use _new and _free for memory allocating constructors and destructors and _init and _destroy for non-memory allocating constructors and destructors.

* Lines must not exceed 120 characters. If a line is too long, it must be broken into several lines.

* Conditional bodies must always use braces, even if they are one-liners.  The only exception that can be is when the conditional is a single line and the body is a single line:

  if (condition) whatever();
