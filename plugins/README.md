Plugins registry for Blosc users
================================

Blosc has a tradition of supporting different filters and codecs for compressing data, and it has been up to the user to choose one or another depending on their needs. However, it is clear that there are always scenarios where a richer variety of them could be useful. Therefore, the Blosc team has set new goals:

1) Implement a way for users to register filters and codecs locally so that they can use them in their setup as needed.

2) Set up a central registry so that other users can use these filters and codecs without interfering with filters and codecs created by other users.

As a bonus, codecs and filters that are accepted into the central registry and meet the quality standards defined in these guidelines will be distributed *within* the C-Blosc2 library. This allows for a much easier way for others to use them; simply install the C-Blosc2 library and you're all set. Of course, to achieve such a status, plugins will require a careful testing process as described below.

Plugin types
--------------

The plugins that are registered in the repository can be codecs or filters.

A codec is a program that compresses and decompresses digital data streams with the objective of reducing dataset size to enable faster transmission of data. Blosc uses various codecs, including BLOSCLZ, LZ4, and ZSTANDARD.

A filter is a program that rearranges data without changing its size, so that the initial and final sizes remain equal. A filter consists of an encoder and decoder. The filter encoder is applied before using the codec compressor (or codec encoder) to make the data easier to compress, while the filter decoder is used after the codec decompressor (or codec decoder) to restore the original data arrangement. Some filters used by Blosc include SHUFFLE, which rearranges data based on the type size, and TRUNC, which zeroes the mantissa bits to reduce the precision of (floating point) data and increase the compression ratio.

Here it is an example on how the compression process goes:


    --------------------   filter encoder  -------------------   codec encoder   -------
    |        src        |   ----------->  |        tmp        |   ---------->   | c_src |
    --------------------                   -------------------                   -------

And the decompression process: 

    --------   codec decoder    -------------------   filter decoder  -------------------
    | c_src |    ----------->  |        tmp        |   ---------->   |        src        |
    --------                    -------------------                   -------------------

Furthermore, during the pipeline process, you can use up to six different filters, ordered in any way you prefer.


Blosc global registered plugins vs user registered plugins
----------------------------------------------------------

**Blosc global registered plugins** are official Blosc plugins that have passed through a selection process
and have been recognised by the Blosc Development Team. These plugins are available for 
everybody in the C-Blosc2 GitHub repository and users can install them anytime.

**User registered plugins** are plugins that users register locally, and they can use them
in the same way as in the examples `urcodecs.c` and `urfilters.c`.

If you only want to use a plugin on your own devices you can just register it as a user registered 
plugin with an ID between *BLOSC2_USER_REGISTERED_FILTERS_START* and *BLOSC2_USER_REGISTERED_FILTERS_STOP*. 
Otherwise, if you think that your plugin could be useful for the community you can apply for 
registering it as an official Blosc plugin following the next steps.


Requirements for registering plugins
------------------------------------

For users wanting to register a new codec or filter, there are some requirements
that their code must satisfy:

- First, the plugin code must be **developed in C**, have a relatively small footprint
  and meet decent quality code standards.

- Second, users must develop a test suite which prove that the plugin works correctly.

Finally, even if these requirements are completely satisfied, it is not
guaranteed that the plugin will be useful or contribute something
different from the existing ones, so the Blosc development team has the final
say and will decide if a plugin is to be accepted or not.


Steps
-----

1. First, tests must be provided and be passing.

   **It is completely mandatory and necessary to add these lines to `main()` in each test to make plugins machinery work:**
   - `blosc2_init()` at the beginning
   - `blosc2_destroy()` in the end

2. Then, the user must make a fork of the C-Blosc2 GitHub repository,
   adding a new folder within the plugin sources to the path `plugins/codecs` or
   `plugins/filters` depending on the plugin type.

3. Furthermore, a text file named `README.rst` must be provided where it is explained:

   * The plugin motivation, why and for what purpose was the plugin created.

   * How to use the plugin.

   * What does the plugin do and how it works.

   * The advantages and disadvantages of the plugin compared to the rest.

4. To register a plugin the user must choose a plugin ID between *BLOSC2_GLOBAL_REGISTERED_FILTERS_START*
   and *BLOSC2_GLOBAL_REGISTERED_FILTERS_STOP* and write it at `include/blosc2/codecs-registry.h` or
   `include/blosc2/filters-registry.h` depending on the plugin type.
   
   Then, you have to edit `plugins/codecs/codecs-registry.c`or `plugins/codecs/filters-registry.c` in the next way:
  
   At the top it must be added `#include "plugin_folder/plugin_header.h"`, and into the register function you must
   follow the same steps that were done for the existing plugins.

5. Finally, the Blosc development team will carry out the evaluation process
   to decide whether the plugin is useful and hence, candidate to be integrated into the C-Blosc2
   source code distribution.  In case of a negative decision, the original author will be informed,
   together with a series of advices for starting a new iteration if desired.


Examples
--------

In the `plugins/` directory there can be found different examples of codecs and filters
available as plugins that can be used in the compression process, and that
can be used as an example on how to implement plugins that can make into C-Blosc2.
Some of these are `ndlz`, `ndcell`, `ndmean` or `bytedelta`.


Thanks
------

We would like to express our gratitude to the NumFOCUS Foundation for providing the funds to implement this functionality.
