=================
Releasing Blosc2
=================

:Author: The Blosc Developers
:Contact: francesc@blosc.org
:Date: 2019-08-09


Preliminaries
-------------

- Make sure that ``RELEASE_NOTES.md`` and ``ANNOUNCE.md`` are up to
  date with the latest news in the release.

- Check that *VERSION* symbols in blosc/blosc.h contains the correct info.

- Commit the changes with::

    $ git commit -a -m "Getting ready for release X.Y.Z"


Testing
-------

Create a new build/ directory, change into it and issue::

  $ cmake ..
  $ cmake --build .
  $ ctest

To actually test Blosc the hard way, look at the end of:

http://blosc.org/synthetic-benchmarks.html

where instructions on how to intensively test (and benchmark) Blosc
are given.

Forward compatibility testing
-----------------------------

First, go to the compat/ directory and generate a file with the current
version::

  $ cd ../compat
  $ export LD_LIBRARY_PATH=../build/blosc
  $ gcc -o filegen filegen.c -L$LD_LIBRARY_PATH -lblosc -I../blosc
  $ ./filegen compress lz4 blosc-lz4-1.y.z.cdata

In order to make sure that we are not breaking forward compatibility,
link and run the `compat/filegen` utility against different versions of
the Blosc library (suggestion: 1.3.0, 1.7.0, 1.11.1, 1.14.1, 2.0.0).

You can compile the utility with different blosc shared libraries with::

  $ export LD_LIBRARY_PATH=shared_blosc_library_path
  $ gcc -o filegen filegen.c -L$LD_LIBRARY_PATH -lblosc -Iblosc.h_include_path

Then, test the file created with the new version with::

  $ ./filegen decompress blosc-lz4-1.y.z.cdata

Repeat this for every codec shipped with Blosc (blosclz, lz4, lz4hc, zlib and
zstd).

Update documentation
--------------------

Go the the `blosc-doc <https://github.com/Blosc/blosc-doc>`_ git repo and update some README files, `blosc2.h` and if API has changed, `c-blosc2_api.rst` too::

  $ cp ../c-blosc2/README.rst src/c-blosc2_files
  $ cp ../c-blosc2/README_CFRAME_FORMAT.rst src/c-blosc2_files
  $ cp ../c-blosc2/README_SFRAME_FORMAT.rst src/c-blosc2_files
  $ cp ../c-blosc2/blosc/blosc2.h src/c-blosc2_files
  $ vi doc/c-blosc2_api.rst  # update with the new API
  $ git commit -a -m"Updated documentation for release X.Y.Z"
  $ git push

After pushing, a new documentation will be rendered at: https://blosc-doc.readthedocs.io/en/latest/
Check that it contains the updated docs.

Tagging
-------

- Create a tag ``X.Y.Z`` from ``master``.  Use the next message::

    $ git tag -a vX.Y.Z -m "Tagging version X.Y.Z"

- Push the tag to the github repo::

    $ git push --tags

- Add the release notes for this tag in the releases tab of github project at:
  https://github.com/Blosc/c-blosc2/releases

Announcing
----------

- Send an announcement to the blosc, pytables-dev, bcolz and
  comp.compression lists.  Use the ``ANNOUNCE.rst`` file as skeleton
  (possibly as the definitive version).


Post-release actions
--------------------

- Edit *VERSION* symbols in blosc/blosc.h in master to increment the
  version to the next minor one (i.e. X.Y.Z --> X.Y.(Z+1).dev).

- Create new headers for adding new features in ``RELEASE_NOTES.rst``
  and empty the release-specific information in ``ANNOUNCE.rst`` and
  add this place-holder instead:

  #XXX version-specific blurb XXX#

- Commit the changes:

  $ git commit -a -m"Post X.Y.Z release actions done"
  $ git push

That's all folks!


.. Local Variables:
.. mode: rst
.. coding: utf-8
.. fill-column: 70
.. End:
