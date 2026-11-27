"""
The function of this file is to generate the needed binaries to appropriately test the blosc2_embed_store. The use instructions are detailed above

## How to use
This file can generate test data for msgpack serialization version 1, version 2 and version 1 schunks. It is done in Python because python-blosc2
was responsible for the implementation of this functionality.

Each `gen` function creates a binary file and outputs some data to the screen that can be copied by the user and pasted in the C code
"""

# /// script
# dependencies = [
#       "msgpack",
#       "numpy",
#       "blosc2"
# ]
# ///

import numpy as np
import msgpack
import blosc2
import struct

def print_keys_C(title: str, items: dict):
    print(f"== {title} ==")
    print(f"static const char* s_{title}[] = {{")
    for i, k in enumerate(items.keys()):
        print(f'    "{k}"', end="")
        if i != len(items) - 1:
            print(",")
        else:
            print("")

    print("};")
    print(f"static const size_t s_{title}_length = {len(items)};")
    print("== END ==\n")

def write_packed(out: str, packed: bytes):
    with open(out, mode="wb") as file:
        file.write(packed)

def gen_v1_msgpack(current_offset: int, items: dict, out_file: str):
    embed_map = {}
    for k, v in items.items():
        first, second = v
        if isinstance(first, str):
            embed_map[k] = {"urlbase": first, "path": second}
        else:
            embed_map[k] = {"offset": first, "length": second}

    packed = msgpack.packb({"current_offset": current_offset, "embed_map": embed_map})
    write_packed(out_file, packed)

def gen_v1_embed_store(items: dict, out_file: str):
    cparams = blosc2.CParams( # BLOSCLZ is required so the tests that remove ZSTD (the default Codec) pass without any problem
        codec=blosc2.Codec.BLOSCLZ,
        clevel=1,
    )
    embed_store = blosc2.EmbedStore(out_file, mode="w", cparams=cparams)
    for k, v in items.items():
        first, second = v
        if isinstance(first, str):
            embed_store[k] = blosc2.C2Array(first, urlbase=second, cparams=cparams)
        else:
            embed_store[k] = np.arange(10)

def uint64(n):
    return b"\xcf" + struct.pack(">Q", n)

def map32_header(n):
    return b"\xdf" + struct.pack(">I", n)

def gen_v2_msgpack(out: str):

    buf = (
        b"\x92"                          # fixarray de 2 elementos
        + uint64(100)                    # 100 como uint64
        + map32_header(4)                # mapa con longitud en 32 bits
        + msgpack.packb("/some/key") + msgpack.packb((10, 10))
        + msgpack.packb("/that/key") + msgpack.packb((20, 30))
        + msgpack.packb("/wait/another/key/?") + msgpack.packb((40, 40))
        + msgpack.packb("/remote") + msgpack.packb(("urlbase", "path"))
    )
    print(f"Buffer: {buf}; len(buf): {len(buf)}")
    print(msgpack.unpackb(buf))

    with open(out, mode="wb") as file:
        file.write(buf)


gen_items = {
    "/first_key": (0, 1),
    "/second_key": (1, 1),
    "/caterva": ("@public/examples/lung-jpeg2000_10x.b2nd", "https://cat2.cloud/demo")
}

if __name__ == "__main__":
    print_keys_C("V1", gen_items)
    gen_v1_msgpack(816, gen_items, "test_embed_store_v1.msgpack")
    gen_v1_embed_store(gen_items, "test_embed_store_v1.b2e")
    gen_v2_msgpack("test_embed_store.msgpack")
