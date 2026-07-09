# Announcing C-Blosc2 3.2.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release adds opt-in cross-process coordination for disk-based
containers shared by several handles (typically several processes), such
as a cache evictor running alongside concurrent readers:

* **Cross-process locking**: an advisory, per-frame lock (a sidecar
  ``.b2lock`` file plus a generation counter) that every participating
  handle can enable via `blosc2_stdio_params.locking` or fleet-wide with
  the `BLOSC_LOCKING` environment variable. A new bracket API,
  `blosc2_schunk_lock()` / `blosc2_schunk_unlock()`, holds the lock across
  several operations so a multi-step mutation is atomic to other locked
  handles; `b2nd_resize()` and `b2nd_set_slice_cbuffer()` now use it
  internally too.
* **Growth-SWMR** (single writer, multiple readers): a reader handle on a
  disk-based frame or b2nd array now follows shape/length growth made by
  another handle on its next access, without reopening — via a new
  `b2nd_refresh()` API and stale-handle coherence fixes that also closed
  gaps in `blosc2_vlmeta_exists()` and the append/insert counters.

Locking is advisory (every handle on a container must opt in) and not
supported over NFS. See the release notes for the full rundown, including
a couple of additive struct fields and a new error code.

This release introduces no breaking API/ABI changes.

For more info, see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high-performance data container optimized for binary data.
Blosc2 is the next generation of Blosc, an
[award-winning library](https://www.blosc.org/posts/prize-push-Blosc2)
that has been around for more than a decade.

Blosc2 expands the capabilities of Blosc by providing a higher level
container that is able to store many chunks on it (hence the super-block name).
It supports storing data on both memory and disk using the same API.
Also, it adds more compressors and filters.

## Download sources

The github repository is over here:

https://github.com/Blosc/c-blosc2

Blosc is distributed using the BSD license, see LICENSE.txt
for details.

## Mailing list

There is an official Blosc mailing list at:

blosc@googlegroups.com
https://groups.google.com/g/blosc


Enjoy Data!
- The Blosc Development Team
