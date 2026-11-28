/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/** @file b2nd_exec.h
 * @brief Blosc2 public array compute execution interface.
 *
 * Self-contained public API over Blosc2's internal array executor. Does not
 * require or include MiniExpr headers. In compression-only C-Blosc2 builds,
 * b2nd_exec_available() returns false and all entry points fail cleanly with
 * B2ND_EXEC_ERR_UNSUPPORTED.
 */

#ifndef BLOSC_PUBLIC_B2ND_EXEC_H
#define BLOSC_PUBLIC_B2ND_EXEC_H

#include "blosc2/blosc2-export.h"
#include "b2nd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define B2ND_EXEC_API_VERSION 1

#ifndef B2ND_EXEC_MAX_NODES
#define B2ND_EXEC_MAX_NODES 1024
#endif
#ifndef B2ND_EXEC_MAX_INPUTS
#define B2ND_EXEC_MAX_INPUTS 128
#endif
#ifndef B2ND_EXEC_MAX_CAPTURES
#define B2ND_EXEC_MAX_CAPTURES 128
#endif
#ifndef B2ND_EXEC_MAX_RANK
#define B2ND_EXEC_MAX_RANK 16
#endif
#ifndef B2ND_EXEC_MAX_STAGES
#define B2ND_EXEC_MAX_STAGES 32
#endif

typedef struct b2nd_plan_s b2nd_plan_t;
typedef struct b2nd_schedule_s b2nd_schedule_t;

typedef enum {
  B2ND_EXEC_SUCCESS = 0,
  B2ND_EXEC_ERR_INVALID_ARG = -1,
  B2ND_EXEC_ERR_UNSUPPORTED = -2,
  B2ND_EXEC_ERR_SOURCE = -3,
  B2ND_EXEC_ERR_SIGNATURE = -4,
  B2ND_EXEC_ERR_EVAL = -5,
  B2ND_EXEC_ERR_OOM = -6
} b2nd_exec_status_t;

enum {
  B2ND_FP_INVALID = 1u,
  B2ND_FP_DIVIDE = 2u,
  B2ND_FP_OVERFLOW = 4u,
  B2ND_FP_UNDERFLOW = 8u
};

typedef struct {
  unsigned flags;
  bool supported;
} b2nd_fp_status_t;

typedef enum {
  B2ND_DTYPE_AUTO = 0,
  B2ND_DTYPE_BOOL = 1,
  B2ND_DTYPE_INT8 = 2,
  B2ND_DTYPE_INT16 = 3,
  B2ND_DTYPE_INT32 = 4,
  B2ND_DTYPE_INT64 = 5,
  B2ND_DTYPE_UINT8 = 6,
  B2ND_DTYPE_UINT16 = 7,
  B2ND_DTYPE_UINT32 = 8,
  B2ND_DTYPE_UINT64 = 9,
  B2ND_DTYPE_FLOAT32 = 10,
  B2ND_DTYPE_FLOAT64 = 11,
  B2ND_DTYPE_COMPLEX64 = 12,
  B2ND_DTYPE_COMPLEX128 = 13,
  B2ND_DTYPE_STRING = 14,
  B2ND_DTYPE_BYTES = 15
} b2nd_exec_dtype_t;

typedef enum {
  B2ND_OP_INPUT = 0,
  B2ND_OP_CAPTURE = 1,
  B2ND_OP_ADD = 2,
  B2ND_OP_SUBTRACT = 3,
  B2ND_OP_MULTIPLY = 4,
  B2ND_OP_NEGATE = 5,
  B2ND_OP_MIN = 6,
  B2ND_OP_SUM = 7,
  B2ND_OP_PROD = 8,
  B2ND_OP_MAX = 9,
  B2ND_OP_ANY = 10,
  B2ND_OP_ALL = 11,
  B2ND_OP_ARGMIN = 12,
  B2ND_OP_ARGMAX = 13,
  B2ND_OP_VAR = 14,
  B2ND_OP_STD = 15,
  B2ND_OP_DIVIDE = 16,
  B2ND_OP_CAST = 17,
  B2ND_OP_FUNCTION = 18,
  B2ND_OP_LT = 19,
  B2ND_OP_GT = 20,
  B2ND_OP_EQ = 21,
  B2ND_OP_NE = 22,
  B2ND_OP_LE = 23,
  B2ND_OP_GE = 24,
  B2ND_OP_BITAND = 25,
  B2ND_OP_BITOR = 26,
  B2ND_OP_BITXOR = 27,
  B2ND_OP_FLOORDIV = 28,
  B2ND_OP_MOD = 29,
  B2ND_OP_POWER = 30,
  B2ND_OP_LSHIFT = 31,
  B2ND_OP_RSHIFT = 32,
  B2ND_OP_INVERT = 33,
  B2ND_OP_NOT = 34,
  B2ND_OP_AND = 35,
  B2ND_OP_OR = 36,
  B2ND_OP_SELECT = 37,
  B2ND_OP_POS = 38
} b2nd_exec_op_t;

typedef enum {
  B2ND_EXEC_POLICY_INTERPRET = 0,
  B2ND_EXEC_POLICY_PREFER_JIT = 1,
  B2ND_EXEC_POLICY_REQUIRE_JIT = 2,
  B2ND_EXEC_POLICY_AUTO = 3
} b2nd_exec_policy_t;

typedef enum {
  B2ND_COMPILER_DEFAULT = 0,
  B2ND_COMPILER_TCC = 1,
  B2ND_COMPILER_CC = 2
} b2nd_compiler_t;

typedef enum {
  B2ND_MATH_DEFAULT = 0,
  B2ND_MATH_AUTO = 1,
  B2ND_MATH_SYSTEM = 2,
  B2ND_MATH_SLEEF = 3,
  B2ND_MATH_ACCELERATE = 4,
  B2ND_MATH_SCALAR = 5
} b2nd_math_backend_t;

typedef enum {
  B2ND_FP_MODE_DEFAULT = 0,
  B2ND_FP_MODE_STRICT = 1,
  B2ND_FP_MODE_CONTRACT = 2,
  B2ND_FP_MODE_FAST = 3
} b2nd_fp_mode_t;

typedef enum {
  B2ND_ACCURACY_DEFAULT = 0,
  B2ND_ACCURACY_U10 = 1,
  B2ND_ACCURACY_U35 = 2
} b2nd_math_accuracy_t;

typedef enum {
  B2ND_CAST_UNSAFE = 0,
  B2ND_CAST_SAME_KIND = 1,
  B2ND_CAST_SAFE = 2
} b2nd_casting_t;

typedef enum {
  B2ND_CARDINALITY_INFER = 0,
  B2ND_CARDINALITY_ELEMENTWISE = 1,
  B2ND_CARDINALITY_BLOCK_SCALAR = 2
} b2nd_cardinality_t;

typedef enum {
  B2ND_REDUCTION_NONE = 0,
  B2ND_REDUCTION_SUM = 1,
  B2ND_REDUCTION_PROD = 2,
  B2ND_REDUCTION_MIN = 3,
  B2ND_REDUCTION_MAX = 4,
  B2ND_REDUCTION_ANY = 5,
  B2ND_REDUCTION_ALL = 6,
  B2ND_REDUCTION_ARGMIN = 7,
  B2ND_REDUCTION_ARGMAX = 8,
  B2ND_REDUCTION_VAR = 9,
  B2ND_REDUCTION_STD = 10
} b2nd_reduction_kind_t;

typedef enum {
  B2ND_EXEC_LANG_AUTO = 0,
  B2ND_EXEC_LANG_FORMULA = 1,
  B2ND_EXEC_LANG_MENUDET_0_1 = 2,
  B2ND_EXEC_LANG_MENUDET_0_2 = 3
} b2nd_exec_lang_t;

enum {
  B2ND_MENUDET_FEATURE_CONTROL_FLOW = 1u << 0,
  B2ND_MENUDET_FEATURE_BLOCK_REDUCTIONS = 1u << 1,
  B2ND_MENUDET_FEATURE_FIXED_STRINGS = 1u << 2,
  B2ND_MENUDET_FEATURE_ND_CONTEXT = 1u << 3,
  B2ND_MENUDET_FEATURE_FLEXIBLE_MATH = 1u << 4
};

typedef enum {
  B2ND_BYTE_ORDER_NATIVE = 0,
  B2ND_BYTE_ORDER_LITTLE = 1,
  B2ND_BYTE_ORDER_BIG = 2
} b2nd_byte_order_t;

typedef enum {
  B2ND_CAPTURE_TYPED = 0,
  B2ND_CAPTURE_WEAK = 1
} b2nd_capture_category_t;

typedef enum {
  B2ND_OPERAND_ARRAY = 0,
  B2ND_OPERAND_VIEW = 1,
  B2ND_OPERAND_SOURCE = 2
} b2nd_operand_kind_t;

typedef enum {
  B2ND_OUTPUT_ARRAY = 0,
  B2ND_OUTPUT_VIEW = 1,
  B2ND_OUTPUT_SINK = 2
} b2nd_output_kind_t;

typedef struct {
  size_t struct_size;
  b2nd_exec_status_t status;
  int line;
  int column;
  uint32_t node;
  size_t stage;
  char message[256];
} b2nd_exec_error_t;

#define B2ND_EXEC_ERROR_INITIALIZER \
  {sizeof(b2nd_exec_error_t), B2ND_EXEC_SUCCESS, 0, 0, UINT32_MAX, SIZE_MAX, {0}}
#define B2ND_EXEC_ERROR_V1_SIZE \
  (offsetof(b2nd_exec_error_t, message) + sizeof(((b2nd_exec_error_t *)0)->message))

typedef struct {
  const char *name;
  b2nd_exec_dtype_t dtype;
  size_t itemsize; /* 0 uses natural width for numeric/bool/complex dtypes. */
} b2nd_exec_input_spec_t;

typedef struct {
  b2nd_exec_input_spec_t spec;
  const void *value;
  b2nd_capture_category_t category;
} b2nd_exec_capture_t;

typedef struct {
  size_t struct_size;
  b2nd_exec_dtype_t dtype;
  size_t itemsize;
  size_t ndim;
  int64_t shape[B2ND_EXEC_MAX_RANK];
} b2nd_exec_input_meta_t;

#define B2ND_EXEC_INPUT_META_INITIALIZER \
  {sizeof(b2nd_exec_input_meta_t), B2ND_DTYPE_AUTO, 0, 0, {0}}
#define B2ND_EXEC_INPUT_META_V1_SIZE \
  (offsetof(b2nd_exec_input_meta_t, shape) + sizeof(((b2nd_exec_input_meta_t *)0)->shape))

typedef struct {
  size_t struct_size;
  b2nd_exec_policy_t execution;
  b2nd_compiler_t compiler;
  b2nd_math_backend_t math_backend;
  b2nd_fp_mode_t fp_mode;
  b2nd_math_accuracy_t math_accuracy;
  b2nd_casting_t casting;
  bool integer_powers;
  bool inherit_environment;
  uint8_t reserved[2];
} b2nd_exec_options_t;

#define B2ND_EXEC_OPTIONS_INITIALIZER \
  {sizeof(b2nd_exec_options_t), B2ND_EXEC_POLICY_INTERPRET, B2ND_COMPILER_DEFAULT, \
   B2ND_MATH_DEFAULT, B2ND_FP_MODE_DEFAULT, B2ND_ACCURACY_DEFAULT, B2ND_CAST_UNSAFE, \
   false, false, {0}}
#define B2ND_EXEC_OPTIONS_V1_SIZE \
  (offsetof(b2nd_exec_options_t, reserved) + sizeof(((b2nd_exec_options_t *)0)->reserved))

typedef struct {
  size_t struct_size;
  b2nd_reduction_kind_t reduction;
  bool has_axes;
  bool keepdims;
  bool has_initial;
  bool has_where_slot;
  size_t naxes;
  int axes[B2ND_EXEC_MAX_RANK];
  b2nd_exec_dtype_t initial_dtype;
  b2nd_capture_category_t initial_category;
  uint8_t initial_scalar[16];
  size_t initial_size;
  b2nd_exec_dtype_t accumulator;
  uint32_t reserved;
  double ddof;
} b2nd_exec_reduce_opts_t;

#define B2ND_EXEC_REDUCE_OPTS_INITIALIZER \
  {sizeof(b2nd_exec_reduce_opts_t), B2ND_REDUCTION_NONE, false, false, false, false, \
   0, {0}, B2ND_DTYPE_AUTO, B2ND_CAPTURE_TYPED, {0}, 0, B2ND_DTYPE_AUTO, 0, 0.0}
#define B2ND_EXEC_REDUCE_OPTS_V1_SIZE \
  (offsetof(b2nd_exec_reduce_opts_t, ddof) + sizeof(((b2nd_exec_reduce_opts_t *)0)->ddof))

typedef struct {
  size_t struct_size;
  b2nd_exec_lang_t lang;
  b2nd_exec_dtype_t output_dtype;
  size_t output_itemsize;
  b2nd_cardinality_t cardinality;
  int context_ndim;
  uint32_t forbidden_features;
  uint32_t reserved;
  const b2nd_exec_capture_t *captures;
  size_t ncaptures;
  b2nd_exec_reduce_opts_t reduce;
} b2nd_expr_options_t;

#define B2ND_EXPR_OPTIONS_INITIALIZER \
  {sizeof(b2nd_expr_options_t), B2ND_EXEC_LANG_AUTO, B2ND_DTYPE_AUTO, 0, \
   B2ND_CARDINALITY_INFER, 0, 0, 0, NULL, 0, B2ND_EXEC_REDUCE_OPTS_INITIALIZER}
#define B2ND_EXPR_OPTIONS_V1_SIZE \
  (offsetof(b2nd_expr_options_t, reduce) + sizeof(((b2nd_expr_options_t *)0)->reduce))

typedef struct {
  size_t struct_size;
  uint32_t id;
  b2nd_exec_op_t op;
  uint32_t args[3];
  size_t nargs;
  uint32_t slot;
  b2nd_exec_dtype_t dtype;
  size_t itemsize;
  uint8_t scalar[16];
  size_t scalar_size;
  b2nd_capture_category_t category;
  bool has_axes;
  bool keepdims;
  bool has_initial;
  bool has_where;
  size_t naxes;
  int axes[B2ND_EXEC_MAX_RANK];
  b2nd_exec_dtype_t initial_dtype;
  b2nd_capture_category_t initial_category;
  uint8_t initial_scalar[16];
  size_t initial_size;
  uint32_t where;
  uint32_t reserved;
  double ddof;
  char function[32];
} b2nd_exec_node_t;

#define B2ND_EXEC_NODE_INITIALIZER \
  {sizeof(b2nd_exec_node_t), 0, B2ND_OP_INPUT, {0, 0, 0}, 0, 0, B2ND_DTYPE_AUTO, 0, \
   {0}, 0, B2ND_CAPTURE_TYPED, false, false, false, false, 0, {0}, B2ND_DTYPE_AUTO, \
   B2ND_CAPTURE_TYPED, {0}, 0, 0, 0, 0.0, {0}}
#define B2ND_EXEC_NODE_V1_SIZE \
  (offsetof(b2nd_exec_node_t, function) + sizeof(((b2nd_exec_node_t *)0)->function))

typedef struct {
  size_t struct_size;
  const b2nd_exec_node_t *nodes;
  size_t nnodes;
  size_t ninputs;
  uint32_t root;
  bool disable_auto_stages;
  uint8_t reserved[3];
} b2nd_exec_graph_t;

#define B2ND_EXEC_GRAPH_INITIALIZER \
  {sizeof(b2nd_exec_graph_t), NULL, 0, 0, 0, false, {0}}
#define B2ND_EXEC_GRAPH_V1_SIZE \
  (offsetof(b2nd_exec_graph_t, reserved) + sizeof(((b2nd_exec_graph_t *)0)->reserved))

typedef struct {
  size_t struct_size;
  const b2nd_plan_t *plan;
  const int *sources; /* -1 - slot: external input; >= 0: earlier stage result. */
  uint32_t node;
  uint32_t reserved;
} b2nd_stage_binding_t;

#define B2ND_STAGE_BINDING_INITIALIZER \
  {sizeof(b2nd_stage_binding_t), NULL, NULL, 0, 0}
#define B2ND_STAGE_BINDING_V1_SIZE \
  (offsetof(b2nd_stage_binding_t, reserved) + sizeof(((b2nd_stage_binding_t *)0)->reserved))

typedef struct {
  size_t struct_size;
  size_t ndim;
  int64_t domain[B2ND_EXEC_MAX_RANK];
  int64_t partitions[B2ND_EXEC_MAX_RANK];
  bool has_selection;
  uint8_t reserved[3];
  int64_t sel_start[B2ND_EXEC_MAX_RANK];
  int64_t sel_stop[B2ND_EXEC_MAX_RANK];
} b2nd_partition_spec_t;

#define B2ND_PARTITION_SPEC_INITIALIZER \
  {sizeof(b2nd_partition_spec_t), 0, {0}, {0}, false, {0}, {0}, {0}}
#define B2ND_PARTITION_SPEC_V1_SIZE \
  (offsetof(b2nd_partition_spec_t, sel_stop) + sizeof(((b2nd_partition_spec_t *)0)->sel_stop))

typedef struct {
  size_t struct_size;
  size_t tile_items;
  size_t max_mapped_bytes;
  size_t max_intermediate_bytes;
  bool pairwise_sum;
  uint8_t reserved[3];
  const b2nd_partition_spec_t *partition_spec;
} b2nd_schedule_options_t;

#define B2ND_SCHEDULE_OPTIONS_INITIALIZER \
  {sizeof(b2nd_schedule_options_t), 4096, (size_t)64 * 1024 * 1024, \
   (size_t)64 * 1024 * 1024, false, {0}, NULL}
#define B2ND_SCHEDULE_OPTIONS_V1_SIZE \
  (offsetof(b2nd_schedule_options_t, partition_spec) + \
   sizeof(((b2nd_schedule_options_t *)0)->partition_spec))

typedef struct {
  size_t struct_size;
  size_t workers;
  size_t max_worker_scratch;
  bool has_worker_scratch;
  uint8_t reserved[3];
  size_t max_decode_bytes;
  size_t max_gather_bytes;
  size_t max_output_bytes;
  unsigned raise_mask;
  uint32_t reserved2;
} b2nd_run_options_t;

#define B2ND_RUN_OPTIONS_INITIALIZER \
  {sizeof(b2nd_run_options_t), 1, 0, false, {0}, (size_t)64 * 1024 * 1024, \
   (size_t)64 * 1024 * 1024, (size_t)64 * 1024 * 1024, 0, 0}
#define B2ND_RUN_OPTIONS_V1_SIZE \
  (offsetof(b2nd_run_options_t, reserved2) + sizeof(((b2nd_run_options_t *)0)->reserved2))

typedef b2nd_exec_status_t (*b2nd_source_read_fn)(
    void *context, const size_t *indices, size_t count,
    void *dst, size_t dst_capacity, b2nd_exec_error_t *error);

typedef b2nd_exec_status_t (*b2nd_sink_write_fn)(
    void *context, size_t offset_items, size_t count,
    const void *src, size_t src_capacity, b2nd_exec_error_t *error);

typedef struct {
  const void *base;
  size_t capacity;
  size_t offset;
  int64_t strides[B2ND_EXEC_MAX_RANK];
  b2nd_byte_order_t byte_order;
  b2nd_exec_dtype_t dtype; /* Optional in b2nd_execute; required in b2nd_eval_into. */
  size_t itemsize;
  size_t ndim;
  int64_t shape[B2ND_EXEC_MAX_RANK];
} b2nd_view_t;

typedef struct {
  b2nd_exec_dtype_t dtype;
  size_t itemsize;
  size_t ndim;
  int64_t shape[B2ND_EXEC_MAX_RANK];
  void *context;
  b2nd_source_read_fn read;
} b2nd_source_t;

typedef struct {
  size_t struct_size;
  b2nd_operand_kind_t kind;
  union {
    const b2nd_array_t *array;
    b2nd_view_t view;
    b2nd_source_t source;
  } as;
} b2nd_operand_t;

#define B2ND_OPERAND_INITIALIZER \
  {sizeof(b2nd_operand_t), B2ND_OPERAND_ARRAY, {NULL}}
#define B2ND_OPERAND_V1_SIZE \
  (offsetof(b2nd_operand_t, as) + sizeof(((b2nd_operand_t *)0)->as))

typedef struct {
  void *base;
  size_t capacity;
  size_t offset;
  int64_t strides[B2ND_EXEC_MAX_RANK];
  size_t staging_budget;
  bool allow_alias;
  uint8_t reserved[3];
} b2nd_output_view_t;

typedef struct {
  b2nd_exec_dtype_t dtype;
  size_t itemsize;
  size_t ndim;
  int64_t shape[B2ND_EXEC_MAX_RANK];
  void *context;
  b2nd_sink_write_fn write;
} b2nd_sink_t;

typedef struct {
  size_t struct_size;
  b2nd_output_kind_t kind;
  union {
    b2nd_array_t *array;
    b2nd_output_view_t view;
    b2nd_sink_t sink;
  } as;
} b2nd_output_t;

#define B2ND_OUTPUT_INITIALIZER \
  {sizeof(b2nd_output_t), B2ND_OUTPUT_ARRAY, {NULL}}
#define B2ND_OUTPUT_V1_SIZE \
  (offsetof(b2nd_output_t, as) + sizeof(((b2nd_output_t *)0)->as))

typedef struct {
  size_t struct_size;
  b2nd_exec_dtype_t dtype;
  size_t itemsize;
  size_t ninputs;
  size_t nstages;
  bool reduction;
  bool identity;
  bool partitionable;
  bool has_jit;
  b2nd_cardinality_t cardinality;
  int context_ndim;
  b2nd_exec_dtype_t map_dtype;
  size_t map_itemsize;
  b2nd_reduction_kind_t reducer;
  size_t plan_metadata_bytes;
  bool has_axes;
  bool keepdims;
  bool has_initial;
  bool has_where;
  size_t naxes;
  int axes[B2ND_EXEC_MAX_RANK];
  size_t where_slot;
  const char *fp_mode;
  const char *math_backend;
  const char *sleef_accuracy;
} b2nd_plan_info_t;

#define B2ND_PLAN_INFO_INITIALIZER \
  {sizeof(b2nd_plan_info_t), B2ND_DTYPE_AUTO, 0, 0, 0, false, false, false, false, \
   B2ND_CARDINALITY_INFER, 0, B2ND_DTYPE_AUTO, 0, B2ND_REDUCTION_NONE, 0, false, \
   false, false, false, 0, {0}, 0, NULL, NULL, NULL}
#define B2ND_PLAN_INFO_V1_SIZE \
  (offsetof(b2nd_plan_info_t, sleef_accuracy) + sizeof(((b2nd_plan_info_t *)0)->sleef_accuracy))

typedef struct {
  size_t struct_size;
  size_t nstages;
  size_t nitems;
  size_t tile_items;
  size_t mapped_bytes;
  size_t reducer_bytes;
  size_t output_items;
  size_t schedule_metadata_bytes;
  size_t ndim;
  int64_t shape[B2ND_EXEC_MAX_RANK];
  size_t broadcast_gather_bytes;
  size_t output_ndim;
  int64_t output_shape[B2ND_EXEC_MAX_RANK];
  size_t reduction_groups;
  size_t reduction_lanes;
  size_t intermediate_bytes;
  b2nd_exec_dtype_t output_dtype;
  size_t output_itemsize;
  bool pairwise_sum;
  bool is_partition_schedule;
  uint8_t reserved[2];
} b2nd_schedule_info_t;

#define B2ND_SCHEDULE_INFO_INITIALIZER \
  {sizeof(b2nd_schedule_info_t), 0, 0, 0, 0, 0, 0, 0, 0, {0}, 0, 0, {0}, 0, 0, 0, \
   B2ND_DTYPE_AUTO, 0, false, false, {0}}
#define B2ND_SCHEDULE_INFO_V1_SIZE \
  (offsetof(b2nd_schedule_info_t, reserved) + sizeof(((b2nd_schedule_info_t *)0)->reserved))

typedef struct {
  size_t struct_size;
  uint32_t node;
  uint32_t reserved;
  size_t last_consumer;
  size_t output_bytes;
  b2nd_plan_info_t plan;
  b2nd_schedule_info_t schedule;
} b2nd_stage_info_t;

#define B2ND_STAGE_INFO_INITIALIZER \
  {sizeof(b2nd_stage_info_t), UINT32_MAX, 0, SIZE_MAX, 0, \
   B2ND_PLAN_INFO_INITIALIZER, B2ND_SCHEDULE_INFO_INITIALIZER}
#define B2ND_STAGE_INFO_V1_SIZE \
  (offsetof(b2nd_stage_info_t, schedule) + sizeof(((b2nd_stage_info_t *)0)->schedule))

typedef struct {
  size_t struct_size;
  size_t tiles;
  size_t map_calls;
  size_t direct_tiles;
  size_t mapped_bytes;
  size_t gather_bytes;
  size_t gathered_bytes;
  size_t interpreter_calls;
  size_t jit_calls;
  size_t read_calls;
  size_t read_items;
  size_t max_read_items;
  size_t read_bytes;
  size_t write_calls;
  size_t write_items;
  size_t max_write_items;
  size_t written_bytes;
  size_t index_buffer_bytes;
  size_t output_buffer_bytes;
  b2nd_fp_status_t fp;
  bool fp_observed;
} b2nd_stage_report_t;

#define B2ND_STAGE_REPORT_INITIALIZER \
  {sizeof(b2nd_stage_report_t), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
   {0, false}, false}
#define B2ND_STAGE_REPORT_V1_SIZE \
  (offsetof(b2nd_stage_report_t, fp_observed) + sizeof(((b2nd_stage_report_t *)0)->fp_observed))

typedef struct {
  size_t struct_size;
  size_t tiles;
  size_t map_calls;
  size_t direct_tiles;
  size_t mapped_bytes;
  b2nd_fp_status_t fp;
  bool fp_observed;
  size_t tasks;
  size_t worker_bytes;
  size_t gather_bytes;
  size_t gathered_bytes;
  size_t staging_bytes;
  size_t interpreter_calls;
  size_t jit_calls;
  size_t decode_bytes;
  size_t storage_metadata_bytes;
  size_t decodes;
  size_t decoded_bytes;
  size_t cache_hits;
  size_t read_spans;
  size_t evaluated_groups;
  size_t max_group_items;
  size_t read_calls;
  size_t read_items;
  size_t max_read_items;
  size_t read_bytes;
  size_t write_calls;
  size_t write_items;
  size_t max_write_items;
  size_t written_bytes;
  size_t index_buffer_bytes;
  size_t output_buffer_bytes;
  size_t entered_stages;
  size_t intermediate_bytes;
  size_t live_intermediate_bytes;
  size_t failed_stage;
  uint32_t failed_node;
  b2nd_stage_report_t *stage_reports;
  size_t stage_reports_cap;
} b2nd_exec_report_t;

#define B2ND_EXEC_REPORT_INITIALIZER \
  {sizeof(b2nd_exec_report_t), 0, 0, 0, 0, {0, false}, false, 0, 0, 0, 0, 0, 0, 0, \
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, SIZE_MAX, \
   UINT32_MAX, NULL, 0}
#define B2ND_EXEC_REPORT_V1_SIZE \
  (offsetof(b2nd_exec_report_t, stage_reports_cap) + \
   sizeof(((b2nd_exec_report_t *)0)->stage_reports_cap))

/**
 * @brief Return whether this C-Blosc2 build includes numerical execution support.
 */
BLOSC_EXPORT bool b2nd_exec_available(void);

/**
 * @brief Compile a formula or Menudet kernel source (with optional reduction options).
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_plan_from_expr(
    const char *source,
    const b2nd_exec_input_spec_t *inputs,
    size_t ninputs,
    const b2nd_exec_options_t *exec_opts,
    const b2nd_expr_options_t *expr_opts,
    b2nd_plan_t **out,
    b2nd_exec_error_t *error);

/**
 * @brief Compile a typed multi-op DAG descriptor (auto-splitting reductions and shared nodes).
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_plan_from_graph(
    const b2nd_exec_graph_t *graph,
    const b2nd_exec_options_t *exec_opts,
    b2nd_plan_t **out,
    b2nd_exec_error_t *error);

/**
 * @brief Compose prepared child plans with external and producer slot bindings into one plan.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_plan_compose(
    const b2nd_stage_binding_t *stages,
    size_t nstages,
    size_t ninputs,
    b2nd_plan_t **out,
    b2nd_exec_error_t *error);

/**
 * @brief Query immutable plan metadata, stage count, reduction properties, and FP/math policy.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_plan_describe(
    const b2nd_plan_t *plan,
    b2nd_plan_info_t *out);

/**
 * @brief Query a single stage's plan metadata, input bindings, and origin node ID.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_plan_describe_stage(
    const b2nd_plan_t *plan,
    size_t stage,
    b2nd_plan_info_t *out,
    int *sources,
    size_t sources_cap,
    size_t *ninputs_out,
    uint32_t *node_out);

/**
 * @brief Release a caller reference to a b2nd_plan_t.
 */
BLOSC_EXPORT void b2nd_plan_free(b2nd_plan_t *plan);

/**
 * @brief Specialize a b2nd_plan_t against input metadata and memory budgets without reading data.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_schedule_create(
    const b2nd_plan_t *plan,
    const b2nd_exec_input_meta_t *inputs,
    size_t ninputs,
    const b2nd_schedule_options_t *options,
    b2nd_schedule_t **out,
    b2nd_exec_error_t *error);

/**
 * @brief Query resolved schedule metadata and optional per-stage schedule information.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_schedule_describe(
    const b2nd_schedule_t *schedule,
    b2nd_schedule_info_t *out,
    b2nd_stage_info_t *stages,
    size_t stages_cap);

/**
 * @brief Perform metadata-only admission to check whether concrete operands/output are supported.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_schedule_admit(
    const b2nd_schedule_t *schedule,
    const b2nd_operand_t *inputs,
    size_t ninputs,
    const b2nd_output_t *output,
    bool *admitted,
    b2nd_exec_error_t *error);

/**
 * @brief Destroy a b2nd_schedule_t and release its retained reference to the parent plan.
 */
BLOSC_EXPORT void b2nd_schedule_free(b2nd_schedule_t *schedule);

/**
 * @brief Execute a specialized schedule over any mix of arrays, resident views, or callbacks.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_execute(
    const b2nd_schedule_t *schedule,
    const b2nd_operand_t *inputs,
    size_t ninputs,
    const b2nd_output_t *output,
    const b2nd_run_options_t *options,
    b2nd_exec_report_t *report,
    b2nd_exec_error_t *error);

/**
 * @brief One-shot compile and evaluate an expression string into a newly allocated b2nd_array_t.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_eval(
    const char *source,
    const char *const *names,
    const b2nd_array_t *const *inputs,
    size_t ninputs,
    const b2nd_exec_options_t *exec_opts,
    const b2nd_expr_options_t *expr_opts,
    const b2nd_schedule_options_t *sched_opts,
    const b2nd_run_options_t *run_opts,
    b2nd_context_t *out_ctx,
    b2nd_array_t **out,
    b2nd_exec_report_t *report,
    b2nd_exec_error_t *error);

/**
 * @brief One-shot compile and evaluate an expression string into a caller-supplied destination.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_eval_into(
    const char *source,
    const char *const *names,
    const b2nd_operand_t *inputs,
    size_t ninputs,
    const b2nd_exec_options_t *exec_opts,
    const b2nd_expr_options_t *expr_opts,
    const b2nd_schedule_options_t *sched_opts,
    const b2nd_run_options_t *run_opts,
    const b2nd_output_t *output,
    b2nd_exec_report_t *report,
    b2nd_exec_error_t *error);

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_PUBLIC_B2ND_EXEC_H */
