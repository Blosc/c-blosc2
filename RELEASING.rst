Releasing a version
===================

Preliminaries
-------------

- Make sure that ``RELEASE_NOTES.md`` and ``ANNOUNCE.md`` are up to
  date with the latest news in the release.

- Check that *VERSION* symbols in include/blosc2.h contains the correct info.

- If API/ABI changes, please increase the minor number (e.g. 2.15 -> 2.16) *and*
  bump the SOVERSION in blosc/CMakeLists.txt.
  When in doubt on when SOVERSION should change, see these nice guidelines:
  https://github.com/conda-forge/c-blosc2-feedstock/issues/62#issuecomment-2049675391

- Commit the changes with::

    $ git commit -a -m "Getting ready for release X.Y.Z"
    $ git push


Testing
-------

Create a new build/ directory, change into it and issue::

  $ cmake ..
  $ cmake --build .
  $ ctest


Forward compatibility testing
-----------------------------

First, go to the compat/ directory and generate a file with the current
version::

  $ cd ../compat
  $ export LD_LIBRARY_PATH=../build/blosc
  $ gcc -o filegen filegen.c -L$LD_LIBRARY_PATH -lblosc2 -I../include
  $ ./filegen compress lz4 blosc-lz4-1.y.z.cdata

In order to make sure that we are not breaking forward compatibility,
link and run the `compat/filegen` utility against different versions of
the Blosc library (suggestion: 1.3.0, 1.7.0, 1.11.1, 1.14.1, 2.0.0).

You can compile the utility with different blosc shared libraries with::

  $ export LD_LIBRARY_PATH=shared_blosc_library_path
  $ gcc -o filegen filegen.c -L$LD_LIBRARY_PATH -lblosc -Iblosc2.h_include_path

Then, test the file created with the new version with::

  $ ./filegen decompress blosc-lz4-1.y.z.cdata

Repeat this for every codec shipped with Blosc (blosclz, lz4, lz4hc, zlib and
zstd).

Tagging
-------

- Create a tag ``X.Y.Z`` from ``master``.  Use the next message::

    $ git tag -a vX.Y.Z -m "Tagging version X.Y.Z"

- Push the tag to the github repo::

    $ git push --tags

- Create a new release visiting https://github.com/Blosc/c-blosc2/releases/new
  and add the release notes copying them from `RELEASE_NOTES.md` document.


Check documentation
-------------------

Go to `blogsite actions <https://github.com/Blosc/blogsite/actions>`_ and trigger a build
by going to the last run and clicking on "Re-run all jobs".

Wait up to 10 min and go to the `blosc2 docs <https://www.blosc.org/c-blosc2/c-blosc2.html>`_
and check that it contains the updated docs.


Announcing
----------

- Send an announcement to the blosc and comp.compression mailing lists.
  Use the ``ANNOUNCE.md`` file as skeleton (likely as the definitive version).

- Toot about it from the @Blosc2 account in https://fosstodon.org.


Post-release actions
--------------------

- Edit *VERSION* symbols in blosc/blosc2.h in master to increment the
  version to the next minor one (i.e. X.Y.Z --> X.Y.(Z+1).dev).

- Create new headers for adding new features in ``RELEASE_NOTES.md``
  and add this place-holder instead:

  #XXX version-specific blurb XXX#

- Commit the changes::

  $ git commit -a -m"Post X.Y.Z release actions done"
  $ git push

That's all folks!


.. Local Variables:
.. mode: rst
.. coding: utf-8
.. fill-column: 70
.. End:
