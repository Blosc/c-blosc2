Blosc2 Extended-Frame
=====================

Overview
--------
The extended-frame implementation allows the storage of different Blosc
data chunks sparse on-disk. An extended-frame is in fact a frame, with the main difference that
its chunk section is only composed of the index chunk. Instead, the data chunks
are stored as independent binary files in the same directory where
the frame file is stored.

To understand it better, a graphic is shown bellow, comparing the
structure of a frame and an eframe.
A frame for sequentially on-disk storage purposes is a binary file
composed of a header, a chunks section and a trailer.
The header contains information needed to decompress the chunks and the
trailer contains a user meta data chunk and a fingerprint.
As seen in the image, the chunks section is composed of all the
data chunks of the frame plus the index chunk. This last one contains
the index to each chunk (where each chunk begins inside the frame file).

However, the frame file of an extended-frame (named as chunks.b2frame)
contains only the index chunk in its chunk section plus the header
and the trailer as well. With the index chunk the name of the chunk
file is obtained by passing it to hexadecimal with a length of 8
characters padded with zeros and adding the `.chunk` extension.

.. image:: C:\Users\Marta\Desktop\framevseframe.svg
  :scale: 50 %

Image Comparative between Frame (left) and Disperse-Frame (right)

Advantages
----------
One advantage of this new feature is that the limitation
regarding the number of chunks is :math:`2^29`, which is
a pretty reasonable number considering that the limitations of the most
used file systems are between :math:`2^16` and :math:`2^32` files
per directory.

Another advantage compared to the frame is the lack of empty
spaces when updating a chunk.
When updating a chunk in a frame, the space from the old chunk
is left useless and the new chunk is added at the end (just before
the index chunk), whereas when updating an eframe
the whole chunkfile is overwritten. To help to understand it
a little more the frame and eframe can be seen as bookshelves.
The first one is a fitted bookshelves. Whereas an eframe can be
seen as regular bookshelves. Both of the bookshelves are wider
enough to fit all the number of files (or books) that the file system
can admit.
Because the frame bookshelves are fitted, they are built to fit books
of 30 cm tall. But what happens when updating a book by its new version
is that the editors decided to change the height of the book to
35 cm.
The new version cannot fit in the bookshelves and an extra shelf is built
to store it leaving the empty space from the old book useless.
However, because the sframe bookshelves have a stantard height
of 40 cm the  new book is replaced by the old one without any useless
space left.



Future work
-----------

In the future instead of storing the chunks in the local computer
they could be stored in another machine and accessed remotely.
That way, with just the metainfo (frame file) we could
access the whole eframe with its data chunks. For a clearer idea
of how much this will mean an eframe of 1000 chunks was created. The
total size of the data chunks was 58 MB and the frames file size was
only 1 KB. This could be surely practical for teleworking. With just an
email of something more than a 1 KB any worker could access all
the data stored in the eframe.

This feature could also be used in the future to implement
remote databases based in . In which the key would be the metainfo plus
chunk index and the value would be the data chunk.

If you are interested in this and want to contribute any help is
welcome (sponsoring, donations, ideas or even some implementations).


