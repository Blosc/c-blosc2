=================
Releasing Blosc2
=================

:Author: Francesc Alted
:Contact: francesc@blosc.org
:Date: 2019-08-09


Preliminaries
-------------

- Make sure that ``RELEASE_NOTES.rst`` and ``ANNOUNCE.rst`` are up to
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


Update documentation
--------------------

Go the the `blosc-doc <https://github.com/Blosc/blosc-doc>`_ git repo and update the `README.rst`
and `blosc2.h` and do a commit::

  $ cp ../c-blosc2/README.rst src/c-blosc2_files/README.rst
  $ cp ../c-blosc2/blosc/blosc2.h src/c-blosc2_files
  $ git commit -a -m"Updated documentation for release X.Y.Z"
  $ pit push

After pushing, a new documentation will be rendered at: https://blosc-doc.readthedocs.io/en/latest/
Check that it contains the updated docs.

Tagging
-------

- Create a tag ``X.Y.Z`` from ``master``.  Use the next message::

    $ git tag -a vX.Y.Z -m "Tagging version X.Y.Z"

- Push the tag to the github repo::

    $ git push --tags


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


That's all folks!


.. Local Variables:
.. mode: rst
.. coding: utf-8
.. fill-column: 70
.. End:
