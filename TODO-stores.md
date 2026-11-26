# TODO Stores

This file documents certain todos regarding stores (EmbedStore, DictStore, ...).

## DictStore

- Decide if, in case a user decides to add an extension to a key, if the store should respect it or not.
  - In case that the store decides to respect it, it must check if it is valid and if the SChunk passed to the function matches the extension
- Right now, the DictStore does normalizes keys to be later used in persistence. The key format should be formalized.
- There are functions in the code that could be generalized into more static functions (like `zip_archive` creation and iteration).
- Add to_b2z and to_b2d when storage type is MEMORY_CONTIGUOUS.
- dstore.urlpath should be an absolute path or a file descriptor (`openat(...)`).

- Decide what to do with blosc2_dict_store_to_buffer. It should accept B2Z and B2D?
- Better path handling

## EmbedStore


