Plugins register for Blosc users
=============================================================================

In the last times/ Recently Blosc has been using a wide range of
codecs and filters for datasets compression. Consequently, the
Blosc Development Team new objective is to register these tools
by using a repository that stores them in form of plugins.


Plugin types
--------------

The plugins that are stored in the repository can be codecs or filters.

A **codec** is a program able to compress and decompress a digital data stream
with the objective of reduce dataset size to enable a faster transmission
of data.
Some of the codecs most used by Blosc are *BLOSCLZ*, *LZ4* and *ZSTANDARD*.

A **filter** is a program that reorders a digital dataset without
changing its size, so that the initial and final size are equal.
A filter consists of encoder and decoder. Filter encoder is applyed before
using codec compressor in order to make data easier to compress and filter
decoder is used after codec decompressor to recover the original data.
Some filters really used by Blosc are *SHUFFLE*, which groups data in an
array based on the typesize, and *TRUNC*, which removes decimal places
from each dataset item.

::

    --------------------   codec encoder   --------
    |       input       |   ---------->   | output |
     -------------------                   --------

     -------------------   filter encoder  -------------------
    |	    input       |   ----------->  |      output	      |
     -------------------                   -------------------


Requirements for adding plugins
~~~~~~~~~~~~~~~~~~~

For users who want to register a new codec or filter, there are some
necessary requirements that their code must satisfy.

- In the first place, the plugin code must be **developed in C**.

- In the second place, users must develop some tests which prove that the plugin works correctly.

- .............FUZZER........................

Finally, even if these requirements are completely satisfied, it is not
guaranteed that the plugin will be useful or contribute something
different to the rest, so the Blosc development team has the final say and
will decide if a plugin is accepted or not.


Steps
~~~~~~~~~~~~~~~~~~~

1. First, testing and fuzzing processes must be successful.

2. Once the plugin is ready, the user must make a fork of the c-blosc2 Github repository, adding a new folder with the plugin sources to the path c-blosc2/plugins/codecs or c-blosc2/plugins/filters depending on the plugin type.

3. Furthermore, the user must create a text file named README where it is explained:

    - The plugin motivation, why and for what purpose was the plugin created.
    - What does the plugin do and how it works.
    - The advantages and disadvantages of the plugin compared to the rest.

4. Finally, the Blosc development team will carry out the evaluation process to decide if the plugin is useful and must be integrated into c-blosc2.


Examples
~~~~~~~~~~~~~~~~~~~

In c-blosc2/plugins there can be found different examples of codecs and filters available as plugins that can be used in the compression process, such as ndlz, ndcell or ndmean.






.

.

.

.

.

APARTADOS
- Tipos de plugins añadibles:
    - Codecs -> explicar y ejemplos (LZ4, BLOSCLZ, ZSTD...)
    - Filtros -> explicar (mismo tamaño ip-op) y ejemplos
                 (SHUFFLE, TRUNC...)

- Requisitos para añadir plugins:
    - Escrito en C
    - Tests
    - Fuzzer
    - Blosc development team se reserva la última palabra

- Pasos:
    1. Tests y fuzzer pasan
    2. Hacer PR y explicar ahí:
        - la motivación del plugin (para qué sirve)
        - ventajas y desventajas del plugin
    3. Proceso de evaluación

- Ejemplos:
    - Filtros: ndcell, ndmean
    - Codecs: ndlz




