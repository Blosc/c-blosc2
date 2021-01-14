Blosc2 Extended-Frame Format
============================

Blosc (as of version 2.0.0) has an extended-frame format that allows for the storage of different Blosc data chunks
sparse, on-disk.

When creating an eframe one must denote the `storage.sequential` as false and provide a name (which will be a directory)
in `storage.urlpath` for the eframe to be stored. It is recommended to name the directory with the `.b2eframe`
extension.

An eframe is made up of a frame file and the chunks stored in the same directory on-disk.
The frame follows the format described in the `frame format <README_FRAME_FORMAT.rst>`_ document, with the difference
that the frame's chunks section is made up only of the index chunk which will have the indexes to each chunk. The frame
file name is always `chunks.b2frame`.

Chunks
------

The chunks are stored in the directory as binary files. Each chunk file name will be composed of the index of the chunk
in hexadecimal written in capital letters with a length of 8 characters padded with zeros with the `.chunk` extension.
As an example, 15 chunks could be named as follows::

 00000000.chunk, 00000001.chunk, ··· , 0000000E.chunk, 0000000F.chunk

Each chunk follows the format described in the
`chunk format <README_CHUNK_FORMAT.rst>`_ document.

*Note:* The real order of the chunks is in the index chunk and may not follow the order of the names.
This can occur when doing an insertion or a reorder. For more information see the **Examples** section below.

Examples
--------

Structure example
^^^^^^^^^^^^^^^^^
As shown below, an eframe of 4 chunks will be composed of a directory with each chunk file and the frame file::

 dir.b2eframe/
 │
 ├── 00000000.chunk
 │
 ├── 00000001.chunk
 │
 ├── 00000002.chunk
 │
 ├── 00000003.chunk
 │
 └── chunks.b2frame


Insertion example
^^^^^^^^^^^^^^^^^
When doing an insertion in the nth position, in the same position
of the index chunk will be the real chunk index which will be the numbers of chunks that there were before
inserting the new one.
Following the previous example, it its shown the content of the directory and the index chunk before and
after an insertion in the 2nd position::

 Before                                 After

 dir.b2eframe/                          dir.b2eframe/
 │                                      │
 ├── 00000000.chunk                     ├── 00000000.chunk
 │                                      │
 ├── 00000001.chunk                     ├── 00000001.chunk
 │                                      │
 ├── 00000002.chunk                     ├── 00000002.chunk
 │                                      │
 ├── 00000003.chunk                     ├── 00000003.chunk
 │                                      │
 └── chunks.b2frame                     ├── 00000004.chunk
                                        │
                                        └── chunks.b2frame

 Possible index                         New index
 chunk content:  [0, 1, 2, 3]           chunk content: [0, 1, 4, 2, 3]

Note that neither the file names nor their contents change, so when accesing the 2nd chunk the
`00000004.chunk` file will be read.

Reorder example
^^^^^^^^^^^^^^^
As in the insertion case, when doing a reorder the chunks names and their contents are not changed, but the content of the
index chunk does. When reordering the chunks, a new order list is passed and the index chunk is changed according to that
list. Following with the first example of this section, the content of the index chunk is shown before and after reordering::

 Before                                 After

 Possible index                         New index
 chunk content:  [0, 1, 2, 3]           chunk content:  [3, 1, 0, 2]
 New order list: [3, 1, 0, 2]

