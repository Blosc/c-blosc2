=============================================================================
C-Blosc2 libraries come with Python-Blosc2 wheels
=============================================================================

C-Blosc2 binary libraries (including plugins functionality) can easily be installed from Python-Blosc2 (>= 0.1.8) wheels:

.. code-block:: console

        $ pip install blosc2
	Collecting blosc2
          Downloading blosc2-0.1.8-cp37-cp37m-manylinux2010_x86_64.whl (3.3 MB)
             |████████████████████████████████| 3.3 MB 4.7 MB/s
        Installing collected packages: blosc2
        Successfully installed blosc2-0.1.8 

As a result, one can easily update to the latest version of C-Blosc2 binaries without the need to manually compile the thing. Following are instructions on how to use the libraries in wheels for different platforms.

Compiling C files with Blosc2 wheels on Windows
----------------------------------------------

- The wheels for Windows have been produced with the Microsoft MSVC compiler, so we recommend that you use it too.  You can get it for free at: https://visualstudio.microsoft.com/es/downloads/.

- In order to check that the MSVC command line is set up correctly, enter ``cl`` in the command prompt window and verify that the output looks something like this:

.. code-block:: console

    > cl
    Microsoft (R) C/C++ Optimizing Compiler Version 19.00.24245 for x64
    Copyright (C) Microsoft Corporation.  All rights reserved.

    usage: cl [ option... ] filename... [ /link linkoption... ]


- Make the compiler available. Its typical installation location uses to be `C:\\Program files (x86)\\Microsoft Visual Studio`, so change your current directory there. Then, to set up the build architecture environment you can open a command prompt window in the `VC\\Auxiliary\\Build` subdirectory and execute `vcvarsall.bat x64` if your architecture is 64 bits or `vcvarsall.bat x86` if it is 32 bits.

- You will need to know the path where the Blosc2 wheel has installed its files.  For this we will use the `dir /s` command (but you can use your preferred location method):

.. code-block:: console

    > dir /s c:\blosc2.lib
     Volume in drive C is OS
     Volume Serial Number is 7A21-A5D5

     Directory of c:\Users\user\miniconda3\Lib

    14/12/2020  09:56             20.848 blosc2.lib
                   1 File(s)           20.848 bytes

         Total list files:
                   1 File(s)          20.848 bytes
                   0 dirs  38.981.902.336 free bytes

- The output shows the path of blosc2.lib in your system, but we are rather interested in the parent one:

.. code-block:: console

    > set WHEEL_DIR=c:\Users\user\miniconda3

- Now, it is important to copy the library `libblosc2.dll` to C:\\Windows\\System32 directory, so it can be found by the executable when it is necessary.

- Finally, to compile C files using Blosc2 libraries, enter this command:

.. code-block:: console

    > cl <file_name>.c <path_of_blosc2.lib> /Ox /Fe<file_name>.exe /I<path_of_blosc2.h> /MT /link/NODEFAULTLIB:MSVCRT

- For instance, in the case of blosc "examples/urcodecs.c":

.. code-block:: console

    > cl urcodecs.c %WHEEL_DIR%\lib\blosc2.lib /Ox /Feurcodecs.exe /I%WHEEL_DIR%\include /MT /link/NODEFAULTLIB:MSVCRT

    Microsoft (R) C/C++ Optimizing Compiler Version 19.10.25017 for x86
    Copyright (C) Microsoft Corporation.  All rights reserved.

    urcodecs.c
    Microsoft (R) Incremental Linker Version 14.10.25017.0
    Copyright (C) Microsoft Corporation.  All rights reserved.

    /out:urcodecs.exe
    /NODEFAULTLIB:MSVCRT
    urcodecs.obj
    /NODEFAULTLIB:MSVCRT
    .\miniconda3\lib\blosc2.lib

- And you can run your program:

.. code-block:: console

    > urcodecs

    Blosc version info: 2.0.0 ($Date:: 2021-05-26 #$)
    Compression ratio: 381.5 MB -> 0.0 MB (14013.5x)
    Compression time: 0.261 s, 1462.1 MB/s
    Decompression time: 0.0669 s, 5698.2 MB/s
    Successful roundtrip data <-> schunk !


Compiling C files with Blosc2 wheels on Linux
---------------------------------------------

- Find the path where Blosc2 wheel has installed its files:

.. code-block:: console

    $ find / -name libblosc2.so 2>/dev/null
    /home/user/miniconda3/lib/libblosc2.so

- The output shows the path of libblosc2.so, but we are rather interested in the parent one:

.. code-block:: console

    $ WHEEL_DIR=/home/user/miniconda3

- To compile C files using Blosc2 you only need to enter the commands:

.. code-block:: console

    $ export LD_LIBRARY_PATH=<path_of_libblosc2.so>
    $ gcc <file_name>.c -I<path_of_blosc2.h> -o <file_name> -L<path_of_libblosc2.so> -lblosc2

- For instance, let's compile blosc's "examples/urcodecs.c":

.. code-block:: console

    $ export LD_LIBRARY_PATH=$WHEEL_DIR/lib   # note that you need the LD_LIBRARY_PATH env variable
    $ gcc urcodecs.c -I$WHEEL_DIR/include -o urcodecs -L$WHEEL_DIR/lib -lblosc2

- Run your program:

.. code-block:: console

    $ ./urcodecs
    Blosc version info: 2.0.0-dev0 ($Date:: 2021-05-26 #$)
    Compression ratio: 381.5 MB -> 0.0 MB (14013.5x)
    Compression time: 1.46 s, 260.7 MB/s
    Decompression time: 0.509 s, 749.1 MB/s
    Successful roundtrip data <-> schunk !

- Rejoice!

