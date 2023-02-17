// Copyright (c) 2019 - 2021, Osamu Watanabe
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
//    modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <cstdint>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include "utils.hpp"

class j2c_src_memory {
 private:
  //  std::unique_ptr<uint8_t[]> buf;
  uint8_t *buf;
  uint32_t pos;
  uint32_t len;

 public:
  j2c_src_memory() {
    buf = nullptr;
    pos = 0;
    len = 0;
  }
  ~j2c_src_memory() {
    if (buf != nullptr) free(buf);
  }
  void alloc_memory(uint32_t length);
  uint8_t get_byte();
  int get_N_byte(uint8_t *buf, uint32_t length);
  uint16_t get_word();
  uint8_t *get_buf_pos() { return (buf + pos); }
  int rewind_2bytes();
  int forward_Nbytes(uint32_t N);
};

class j2c_dst_memory {
 private:
  std::vector<uint8_t> buf;
  uint32_t pos;
  bool is_flushed;

 public:
  j2c_dst_memory() : pos(0), is_flushed(false) {}
  ~j2c_dst_memory() = default;
  int32_t put_byte(uint8_t byte);
  int32_t put_word(uint16_t word);
  int32_t put_dword(uint32_t dword);
  int32_t put_N_bytes(uint8_t *src, uint32_t length);
  int32_t flush(std::ofstream &dst);
  int32_t flush(std::vector<uint8_t> *obuf);
  [[nodiscard]] size_t get_length() const;
  [[maybe_unused]] void print_bytes();
};

class buf_chain {
 private:
  size_t node_pos;
  size_t pos;
  uint32_t total_length;

  std::vector<uint8_t *> node_buf;
  std::vector<uint32_t> node_length;
  uint32_t num_nodes;

  uint8_t *current_buf;
  uint32_t current_length;
  uint8_t tmp_byte;
  uint8_t last_byte;
  uint8_t bits;

 public:
  buf_chain() = default;
  //      : node_pos(0),
  //        pos(0),
  //        total_length(0),
  //        node_buf({}),
  //        node_length({}),
  //        num_nodes(0),
  //        current_buf(nullptr),
  //        current_length(0),
  //        tmp_byte(0),
  //        last_byte(0),
  //        bits(0) {}
  explicit buf_chain(uint32_t num)
      : node_pos(0),
        pos(0),
        total_length(0),
        num_nodes(num),
        current_buf(nullptr),
        current_length(0),
        tmp_byte(0),
        last_byte(0),
        bits(0) {
    for (uint32_t i = 0; i < num; ++i) {
      node_buf.push_back(nullptr);
      node_length.push_back(0);
    }
  }
  buf_chain operator=(buf_chain) = delete;
  buf_chain(buf_chain &&)        = default;
  buf_chain &operator=(const buf_chain &bc) {
    if (this != &bc) {
      this->total_length   = bc.total_length;
      this->num_nodes      = bc.num_nodes;
      this->node_pos       = bc.node_pos;
      this->pos            = bc.pos;
      this->current_length = bc.current_length;
      this->tmp_byte       = bc.tmp_byte;
      this->last_byte      = bc.last_byte;
      this->bits           = bc.bits;
      this->node_buf.reserve(bc.node_buf.size());
      for (size_t i = 0; i < bc.node_buf.size(); ++i) {
        this->node_buf.push_back(bc.node_buf[i]);
        this->node_length.push_back(bc.node_length[i]);
      }
    }
    return *this;
  }
  void add_buf_node(uint8_t *buf, uint32_t len) {
    total_length += len;
    node_buf.push_back(buf);
    node_length.push_back(len);
    num_nodes++;
  }
  void set_buf_node(uint32_t index, uint8_t *buf, uint32_t len) {
    total_length += len;
    node_buf[index]    = buf;
    node_length[index] = len;
  }
  void activate() {
    pos            = 0;
    node_pos       = 0;
    current_buf    = node_buf[0];
    current_length = node_length[0];
  }
  void activate(size_t n) {
    assert(n < this->node_buf.size());
    pos            = 0;
    node_pos       = n;
    current_buf    = node_buf[node_pos];
    current_length = node_length[node_pos];
  }
  void flush_bits() { bits = 0; }
  void check_last_FF() {
    if (tmp_byte == 0xFF) {
      this->get_bit();
    }
  }
  [[nodiscard]] uint32_t get_total_length() const { return total_length; }

  [[maybe_unused]] uint8_t get_specific_byte(uint32_t bufpos) { return *(current_buf + bufpos); }
  uint8_t get_byte() {
    if (pos > current_length - 1) {
      node_pos++;
      assert(node_pos <= num_nodes);
      current_buf    = node_buf[node_pos];
      current_length = node_length[node_pos];
      pos            = 0;
    }
    return *(current_buf + pos++);
  }

  [[maybe_unused]] uint8_t *get_current_address() {
    if (pos > current_length - 1) {
      node_pos++;
      assert(node_pos <= num_nodes);
      current_buf    = node_buf[node_pos];
      current_length = node_length[node_pos];
      pos            = 0;
    }
    return (current_buf + pos++);
  }

  void copy_N_bytes(uint8_t *&buf, uint32_t N) {
    // The first input argument is a reference of pointer.
    // The reason of this is the address of 'buf' shall be increased by N.
    assert((pos + N) <= current_length);
    memcpy(buf, current_buf + pos, N);
    pos += N;
    buf += N;
  }

  uint16_t get_word() {
    uint16_t word = get_byte();
    // word <<= 8;
    // word += get_byte();
    word = static_cast<uint16_t>(word << 8);
    word = static_cast<uint16_t>(word + get_byte());
    return word;
  }
  uint8_t get_bit() {
    if (bits == 0) {
      tmp_byte = get_byte();
      if (last_byte == 255) {
        bits = 7;
      } else {
        bits = 8;
      }
      last_byte = tmp_byte;
    }
    bits--;
    return (tmp_byte >> bits) & 1;
  }
  uint32_t get_N_bits(uint8_t N) {
    uint32_t cwd = 0;
    uint8_t bit;
    for (int i = 0; i < N; i++) {
      bit = get_bit();
      cwd <<= 1;
      cwd += static_cast<uint32_t>(bit);
    }
    return cwd;
  }
};

class packet_header_writer {
 private:
  std::vector<uint8_t> buf;
  uint8_t tmp;
  uint8_t last;
  uint8_t bits;
  uint32_t pos;

 public:
  packet_header_writer() : tmp(0), last(0), bits(8), pos(0) {
    buf.reserve(512);
    // we don't use empty packet
    put_bit(1);
  };

  [[nodiscard]] uint32_t get_length() const { return pos; }

  size_t copy_buf(uint8_t *p) {
    for (size_t i = 0; i < buf.size(); ++i) {
      p[i] = buf[i];
    }
    return buf.size();
  }

  void put_bit(uint8_t b) {
    if (bits == 0) {
      last = tmp;
      // if the last byte was 0xFF, next 1 bit shall be skipped (bit-stuffing)
      bits = (last == 0xFF) ? 7 : 8;
      buf.push_back(tmp);
      pos++;
      tmp = 0;
    }
    bits--;
    tmp = static_cast<uint8_t>(tmp + (b << bits));
  }

  void put_Nbits(uint32_t cwd, uint8_t n) {
    for (int i = n - 1; i >= 0; --i) {
      put_bit((cwd >> i) & 1);
    }
  }

  void flush(bool use_EPH = false) {
    for (int i = 0; i < bits; ++i) {
      put_bit(0);
    }
    buf.push_back(tmp);
    pos++;
    // if the last byte was 0xFF, next 1 bit shall be skipped (bit-stuffing)
    if (tmp == 0xFF) {
      buf.push_back(0);
      pos++;
    }
    if (use_EPH) {
      // write EPH marker
      buf.push_back(0xFF);
      buf.push_back(0x92);
      pos += 2;
    }
  }
};

class tagtree_node {
 private:
  uint8_t level;
  int32_t index;
  int32_t parent_index;
  std::vector<int32_t> child_index;
  uint8_t state;
  uint16_t current_value;
  uint16_t value;
  bool set_flag;  // only for encoder

 public:
  tagtree_node() {
    level         = 0;
    index         = -1;
    parent_index  = 0;
    child_index   = {};
    state         = 0;
    current_value = 0;
    value         = 0;
    set_flag      = false;
  }
  void set_node(uint32_t l, int32_t i, int32_t pi) {
    if (l > 255) {
      printf("ERROR: Specified level for tagtree node is too large.\n");
      throw std::exception();
    }
    level        = static_cast<uint8_t>(l);
    index        = i;
    parent_index = pi;
  }
  void add_child(int32_t val = 0) { child_index.push_back(val); }
  [[nodiscard]] uint8_t get_level() const { return level; }
  [[nodiscard]] int32_t get_index() const { return index; }
  [[nodiscard]] int32_t get_parent_index() const { return parent_index; }
  std::vector<int32_t> get_child_index() { return child_index; }
  [[nodiscard]] uint8_t get_state() const { return state; }
  void set_state(uint8_t s) { state = s; }
  [[nodiscard]] uint16_t get_current_value() const { return current_value; }
  void set_current_value(uint16_t cv) { current_value = cv; }
  [[nodiscard]] uint16_t get_value() const { return value; }
  void set_value(uint16_t v) {
    value    = v;
    set_flag = true;
  }
  [[nodiscard]] bool is_set() const { return set_flag; }
};

class tagtree {
 public:
  uint8_t level;
  std::unique_ptr<tagtree_node[]> node;
  uint32_t num_nodes;
  const uint32_t num_cblk_x;
  const uint32_t num_cblk_y;

 public:
  // tagtree() {
  //   level      = 0;
  //   node       = nullptr;
  //   num_nodes  = 0;
  //   num_cblk_x = 0;
  //   num_cblk_y = 0;
  // }
  tagtree(const uint32_t nx, const uint32_t ny) : level(1), num_nodes(0), num_cblk_x(nx), num_cblk_y(ny) {
    int32_t num_nodes_current_level, width_current_level, height_current_level;
    width_current_level  = (int32_t)num_cblk_x;
    height_current_level = (int32_t)num_cblk_y;

    // calculate number of nodes
    while (true) {
      num_nodes_current_level = width_current_level * height_current_level;
      num_nodes += static_cast<uint32_t>(num_nodes_current_level);
      width_current_level  = ceil_int(width_current_level, 2);
      height_current_level = ceil_int(height_current_level, 2);
      if (num_nodes_current_level <= 1) {
        break;
      } else {
        level++;
      }
    }
    // create tagtree nodes
    node = MAKE_UNIQUE<tagtree_node[]>(num_nodes);

    // build tagtree structure
    int32_t node_index = 0, parent_index, row_parent_index, parent_num = 0;
    uint32_t depth       = static_cast<uint32_t>(level - 1);
    width_current_level  = (int32_t)num_cblk_x;
    height_current_level = (int32_t)num_cblk_y;
    tagtree_node *current_node, *parent_node;
    current_node = &node[static_cast<size_t>(node_index)];
    while (true) {
      num_nodes_current_level = width_current_level * height_current_level;
      if (num_nodes_current_level <= 1) {
        break;
      }
      parent_num += num_nodes_current_level;
      row_parent_index = parent_num;
      for (int32_t i = 0; i < height_current_level; i++) {
        parent_index = row_parent_index;
        for (int32_t j = 0; j < width_current_level; j++) {
          current_node->set_node(depth, node_index, parent_index);
          parent_node = &node[static_cast<size_t>(parent_index)];
          parent_node->add_child(node_index);
          node_index++;
          current_node = &node[static_cast<size_t>(node_index)];

          if (j % 2 == 1 && j != width_current_level - 1) {
            parent_index++;  // move to next parent in horizontal
          }
        }
        if (i % 2 == 1) {
          row_parent_index += ceil_int(width_current_level, 2);  // move to next parent in vertical
        }
      }
      width_current_level  = ceil_int(width_current_level, 2);  // number of horizontal nodes for next level
      height_current_level = ceil_int(height_current_level, 2);  // number of vertical nodes for next level
      depth--;
    }
    // when we get here, root node should be set.
    current_node = &node[num_nodes - 1];
    current_node->set_node(depth, node_index,
                           -1);  // parent index = - 1 means I am the ROOT
  }
  void build() const {
    for (uint32_t i = 0; i < this->num_nodes; ++i) {
      tagtree_node *current = &this->node[i];
      // need to reset current value because packet header generation will be done twice
      current->set_current_value(0);
      current->set_state(0);
      if (!current->is_set()) {
        std::vector<int32_t> children = current->get_child_index();
        uint16_t val                  = this->node[static_cast<size_t>(children[0])].get_value();
        for (int &j : children) {
          uint16_t tmp = this->node[static_cast<size_t>(j)].get_value();
          val          = (val > tmp) ? tmp : val;
        }
        current->set_value(val);
      }
    }
  }
  // tagtree &operator=(const tagtree &bc) {
  //   level      = bc.level;
  //   num_nodes  = bc.num_nodes;
  //   num_cblk_x = bc.num_cblk_x;
  //   num_cblk_y = bc.num_cblk_y;
  //   node       = MAKE_UNIQUE<tagtree_node[]>(num_nodes);
  //   for (unsigned long i = 0; i < num_nodes; ++i) {
  //     node[i] = bc.node[i];
  //   }
  //   return *this;
  // }
};
