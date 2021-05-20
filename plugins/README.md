Plugins register for Blosc users
=============================================================================

Blosc has a tradition of supporting different filters and codecs for compressing data,
and it was up to the user to choose one or another depending on her needs.
However, it is clear that there will always be scenarios where a more richer variety
of them could be useful.  So the Blosc Development Team has set a new goals:

1) Implement a way for users to locally register filters and codecs so that they can use
   them in their setup at will.

2) Setup a central registry so that *other* users can make use of these filters and codecs
   without intefering with others that have been created by others.

As a bonus, those filters accepted in the central registry and meeting the quality standards
defined in these guidelines will be distributed *inside* the C-Blosc2 library,
allowing a much easier way for others to use them: install C-Blosc2 library and you are all set.
Of course, to achieve such a status, plugins will require a careful testing process described below.


Plugin types
--------------

The plugins that are stored in the repository can be codecs or filters.

A **codec** is a program able to compress and decompress a digital data stream
with the objective of reduce dataset size to enable a faster transmission
of data.
Some of the codecs used by Blosc are e.g. *BLOSCLZ*, *LZ4* and *ZSTANDARD*.

A **filter** is a program that reorders the data without
changing its size, so that the initial and final size are equal.
A filter consists of encoder and decoder. Filter encoder is applyed before
using the codec compressor (or codec encoder) in order to make data easier to compress
and filter decoder is used after codec decompressor (or codec decoder) to recover
the original data arrangement.
Some filters actually used by Blosc are e.g. *SHUFFLE*, which rearranges data 
based on the typesize, or *TRUNC*, which zeroes mantissa bits so as to reduce
the precision of (floating point) data, and hence, increase the compression ratio.

Here it is an example on how the compression process goes::


    --------------------   filter encoder  -------------------   codec encoder   -------
    |        src        |   ----------->  |        tmp        |   ---------->   | c_src |
    --------------------                   -------------------                   -------

And the decompression process::

    --------   codec decoder    -------------------   filter decoder  -------------------
    | c_src |    ----------->  |        tmp        |   ---------->   |        src        |
    --------                    -------------------                   -------------------

Moreover, during the pipeline process you can use even 6 different 
filters ordered as you prefer.


Requirements for adding plugins
-------------------------------

For users wanting to register a new codec or filter, there are some requirements
that their code must satisfy:

- First, the plugin code must be **developed in C**, have a relatively small footprint
  and meet decent quality code standards.

- Second, users must develop a test suite which prove that the plugin works correctly.

- .............FUZZER........................

Finally, even if these requirements are completely satisfied, it is not
guaranteed that the plugin will be useful or contribute something
different to the existing ones, so the Blosc development team has the final
say and will decide if a plugin is to be accepted or not.


Steps
-----

1. First, both regular tests and fuzzing tests must be provided and be passing.

2. Then, the user must make a fork of the C-Blosc2 Github repository,
   adding a new folder within the plugin sources to the path `plugins/codecs` or
   `plugins/filters` depending on the plugin type.

3. Furthermore, a text file named `README.rst` must be provided where it is explained:

   * The plugin motivation, why and for what purpose was the plugin created.

   * How to use the plugin.

   * What does the plugin do and how it works.

   * The advantages and disadvantages of the plugin compared to the rest.

4. Finally, the Blosc development team will carry out the evaluation process
   (probably via a votation process, with the BDFL having the last say in case of the team is undecided)
   so as to decide whether the plugin is useful and hence, candidate to be integrated into the C-Blosc2
   source code distribution.


Examples
--------

In the `plugins/` directory there can be found different examples of codecs and filters
available as plugins that can be used in the compression process, and that
can be used as an example on how to implement plugins that can make into C-Blosc2.
Some of these examples are `ndlz`, `ndcell` or `ndmean`.



