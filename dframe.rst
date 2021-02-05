Blosc2  Sparse Frame
====================

Overview
--------
The sparse frame implementation allows the storage of different Blosc
data chunks sparse on-disk. For this feature it is used a frame file, which
was implemented for sequentially on-disk storage purposes.

A frame file is a binary file
composed of a header, a chunks section and a trailer.
The header contains information needed to decompress the chunks and the
trailer contains a user meta data chunk and a fingerprint.
As seen in the comparative image below (on the left the frame
structure and on the right the sparse frame structure), the chunks section is composed of all the
data chunks of the frame plus the index chunk. This last one contains
the index to each chunk (where each chunk begins inside the frame file).
All of this is stored sequentially, that means one part is followed
by the next one without (initially) any empty spaces.

However, in a sparse frame the chunks are already stored in a
specified directory as independent binary files.
But there is still the
need to store the information to decompress the chunks
as well as a place to store user meta data.
This is stored in the `chunks.b2frame`,
which is in fact a frame file with the main difference that its
chunks section is only composed of the index chunk. Thus, as seen
in the image following, the `chunks.b2frame`
file is composed of the header, the index chunk and the trailer.
The index chunk contains in each position the identifier
of each chunk which is a number from 0 to :math:`2^29-1`.
With that number the name of the chunk
file is obtained by passing it to hexadecimal with a length of 8
characters padded with zeros and adding the `.chunk` extension.
For example, if the index chunk is 46 (2E in hexadecimal)
the chunk file name would be
`0000002E.chunk`.

.. image:: C:\Users\Marta\Desktop\framevsSframecopy.svg
  :width: 70%
  :align: center


Advantages
----------
One advantage of this new feature is that the limitation
regarding the number of chunks is :math:`2^29`, which is
a pretty reasonable number considering that the limitations of the most
used file systems are between :math:`2^16` and :math:`2^32` files
per directory.

Another advantage compared with the frame is the lack of empty
spaces when updating a chunk.
To illustrate how a frame and a sparse frame behave when updating
a chunk an example is used  for each case.

The set of the data chunks from a frame could be structured as the
`Jenga board game tower <https://en.wikipedia.org/wiki/Jenga>`_, a tower
built with wood blocks but, in constrast to the genuine
`Jenga board game`, not all the
blocks have the same size. Below is showed the initially structure
of this tower. If the orange piece is updated (changed by another
piece) there are two possibilities. The first one is that the new piece
fits into the empty space left where the old piece was. In that case,
the new piece is put in the previous space without any problem.
However, if the new piece
does not fit into the empty space left, the new piece is placed at the
top of the tower (like in the game) leaving the space
where the old piece was empty.

.. image:: C:\Users\Marta\Desktop\jenga3.svg
  :width: 50%
  :align: center

On the other hand, the chunks of a sparse frame can be seen as books. So the
chunks structure could be seen as a bookshelf in which each book
is a different chunk. If it is needed to update one book with
the new edition, one only has to grab the old edition and
replace it by the new one. And the books on the right are moved
so that there is the exact space needed for the new edition. In the
following image the yellow book is replaced by the maroon with the
green rim. Note there is not a single empty space between
the books after replacing the book.

.. image:: C:\Users\Marta\Desktop\bookshelf.svg
  :width: 50%
  :align: center

Future work
-----------

This implementation opens doors to another interesting features made
with a little bit more of work.

For example, adjusting the code to work in the network,
instead of storing the chunks in a local computer
they could be stored in another machine and accessed remotely.
That way, with just the metainfo (the frame file) we could
access the whole sparse frame with its data chunks.
For a clearer idea
of how much this will mean a sparse frame of 1000 chunks was created.
The
total size of the data chunks from this sparse frame
was 58 MB and the frames file size was
only 1 KB. This could be surely practical for teleworking. With just an
email of something more than a 1 KB any worker could access all
the data stored in the sparse frame.

This feature could also be used in the future to implement
remote databases. These databases could be composed
of a key assigned to a value. The key is an identifier for each element
of the database, therefore it must be unique. Whereas the value
is just the information accessed to each key. Similar to a set of unique
keys and a set of doors. Each key can only open one door in particular,
but the room behind the door may or may not be an exact copy of another
one. In this case, the key would be composed of the metainfo plus the index of the chunk
(so that each key is unique) and the value would be the data chunk.


