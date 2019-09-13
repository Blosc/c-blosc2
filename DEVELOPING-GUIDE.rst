Some conventions used in C-Blosc2
=================================

* Use C99 designated initialization whenever possible (specially in examples).

* Use _new and _free for memory allocating constructors and destructors and _init and _destroy for non-memory allocating constructors and destructors.


Naming things
-------------

Naming is one of the most time-consuming tasks, but critical for communicating effectively.  Here it is a preliminary list of names that I am not comfortable with:

* We are currently calling `filters` to a data transformation function that essentially produces the same amount of data, but with bytes shuffled or transformed in different ways.  Perhaps `transformers` would be a better name?
