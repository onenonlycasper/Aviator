// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all of the flags.  It is separated into different section,
// for Debug, Release, Logging and Profiling, etc.  To add a new flag, find the
// correct section, and use one of the DEFINE_ macros, without a trailing ';'.
//
// This include does not have a guard, because it is a template-style include,
// which can be included multiple times in different modes.  It expects to have
// a mode defined before it's included.  The modes are FLAG_MODE_... below:

// We want to declare the names of the variables for the header file.  Normally
// this will just be an extern declaration, but for a readonly flag we let the
// compiler make better optimizations by giving it the value.
#if defined(FLAG_MODE_DECLARE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  extern ctype FLAG_##nam;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt) \
  static ctype const FLAG_##nam = def;

// We want to supply the actual storage and value for the flag variable in the
// .cc file.  We only do this for writable flags.
#elif defined(FLAG_MODE_DEFINE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  ctype FLAG_##nam = def;

// We need to define all of our default values so that the Flag structure can
// access them by pointer.  These are just used internally inside of one .cc,
// for MODE_META, so there is no impact on the flags interface.
#elif defined(FLAG_MODE_DEFINE_DEFAULTS)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  static ctype const FLAGDEFAULT_##nam = def;

// We want to write entries into our meta data table, for internal parsing and
// printing / etc in the flag parser code.  We only do this for writable flags.
#elif defined(FLAG_MODE_META)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  { Flag::TYPE_##ftype, #nam, &FLAG_##nam, &FLAGDEFAULT_##nam, cmt, false },
#define FLAG_ALIAS(ftype, ctype, alias, nam) \
  { Flag::TYPE_##ftype, #alias, &FLAG_##nam, &FLAGDEFAULT_##nam, \
    "alias for --"#nam, false },

// We produce the code to set flags when it is implied by another flag.
#elif defined(FLAG_MODE_DEFINE_IMPLICATIONS)
#define DEFINE_implication(whenflag, thenflag) \
  if (FLAG_##whenflag) FLAG_##thenflag = true;

#define DEFINE_neg_implication(whenflag, thenflag) \
  if (FLAG_##whenflag) FLAG_##thenflag = false;

#else
#error No mode supplied when including flags.defs
#endif

// Dummy defines for modes where it is not relevant.
#ifndef FLAG_FULL
#define FLAG_FULL(ftype, ctype, nam, def, cmt)
#endif

#ifndef FLAG_READONLY
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)
#endif

#ifndef FLAG_ALIAS
#define FLAG_ALIAS(ftype, ctype, alias, nam)
#endif

#ifndef DEFINE_implication
#define DEFINE_implication(whenflag, thenflag)
#endif

#ifndef DEFINE_neg_implication
#define DEFINE_neg_implication(whenflag, thenflag)
#endif

#define COMMA ,

#ifdef FLAG_MODE_DECLARE
// Structure used to hold a collection of arguments to the JavaScript code.
struct JSArguments {
public:
  inline const char*& operator[] (int idx) const {
    return argv[idx];
  }
  static JSArguments Create(int argc, const char** argv) {
    JSArguments args;
    args.argc = argc;
    args.argv = argv;
    return args;
  }
  int argc;
  const char** argv;
};

struct MaybeBoolFlag {
  static MaybeBoolFlag Create(bool has_value, bool value) {
    MaybeBoolFlag flag;
    flag.has_value = has_value;
    flag.value = value;
    return flag;
  }
  bool has_value;
  bool value;
};
#endif

#if (defined CAN_USE_VFP3_INSTRUCTIONS) || !(defined ARM_TEST)
# define ENABLE_VFP3_DEFAULT true
#else
# define ENABLE_VFP3_DEFAULT false
#endif
#if (defined CAN_USE_ARMV7_INSTRUCTIONS) || !(defined ARM_TEST)
# define ENABLE_ARMV7_DEFAULT true
#else
# define ENABLE_ARMV7_DEFAULT false
#endif
#if (defined CAN_USE_VFP32DREGS) || !(defined ARM_TEST)
# define ENABLE_32DREGS_DEFAULT true
#else
# define ENABLE_32DREGS_DEFAULT false
#endif
#if (defined CAN_USE_NEON) || !(defined ARM_TEST)
# define ENABLE_NEON_DEFAULT true
#else
# define ENABLE_NEON_DEFAULT false
#endif

#define DEFINE_bool(nam, def, cmt)   FLAG(BOOL, bool, nam, def, cmt)
#define DEFINE_maybe_bool(nam, cmt)  FLAG(MAYBE_BOOL, MaybeBoolFlag, nam,  \
                                          { false COMMA false }, cmt)
#define DEFINE_int(nam, def, cmt)    FLAG(INT, int, nam, def, cmt)
#define DEFINE_float(nam, def, cmt)  FLAG(FLOAT, double, nam, def, cmt)
#define DEFINE_string(nam, def, cmt) FLAG(STRING, const char*, nam, def, cmt)
#define DEFINE_args(nam, cmt)        FLAG(ARGS, JSArguments, nam, \
                                          { 0 COMMA NULL }, cmt)

#define DEFINE_ALIAS_bool(alias, nam)  FLAG_ALIAS(BOOL, bool, alias, nam)
#define DEFINE_ALIAS_int(alias, nam)   FLAG_ALIAS(INT, int, alias, nam)
#define DEFINE_ALIAS_float(alias, nam) FLAG_ALIAS(FLOAT, double, alias, nam)
#define DEFINE_ALIAS_string(alias, nam) \
  FLAG_ALIAS(STRING, const char*, alias, nam)
#define DEFINE_ALIAS_args(alias, nam)  FLAG_ALIAS(ARGS, JSArguments, alias, nam)

//
// Flags in all modes.
//
#define FLAG FLAG_FULL

// Flags for language modes and experimental language features.
DEFINE_bool(use_strict, false, "enforce strict mode")
DEFINE_bool(es_staging, false, "enable upcoming ES6+ features")

DEFINE_bool(harmony_typeof, false, "enable harmony semantics for typeof")
DEFINE_bool(harmony_scoping, false, "enable harmony block scoping")
DEFINE_bool(harmony_modules, false,
            "enable harmony modules (implies block scoping)")
DEFINE_bool(harmony_symbols, false, "enable harmony symbols")
DEFINE_bool(harmony_proxies, false, "enable harmony proxies")
DEFINE_bool(harmony_collections, false,
            "enable harmony collections (sets, maps)")
DEFINE_bool(harmony_generators, false, "enable harmony generators")
DEFINE_bool(harmony_iteration, false, "enable harmony iteration (for-of)")
DEFINE_bool(harmony_numeric_literals, false,
            "enable harmony numeric literals (0o77, 0b11)")
DEFINE_bool(harmony_strings, false, "enable harmony string")
DEFINE_bool(harmony_arrays, false, "enable harmony arrays")
DEFINE_bool(harmony_maths, false, "enable harmony math functions")
DEFINE_bool(harmony, false, "enable all harmony features (except typeof)")

DEFINE_implication(harmony, harmony_scoping)
DEFINE_implication(harmony, harmony_modules)
DEFINE_implication(harmony, harmony_proxies)
DEFINE_implication(harmony, harmony_collections)
DEFINE_implication(harmony, harmony_generators)
DEFINE_implication(harmony, harmony_iteration)
DEFINE_implication(harmony, harmony_numeric_literals)
DEFINE_implication(harmony, harmony_strings)
DEFINE_implication(harmony, harmony_arrays)
DEFINE_implication(harmony_modules, harmony_scoping)
DEFINE_implication(harmony_collections, harmony_symbols)
DEFINE_implication(harmony_generators, harmony_symbols)
DEFINE_implication(harmony_iteration, harmony_symbols)

DEFINE_implication(harmony, es_staging)
DEFINE_implication(es_staging, harmony_maths)
DEFINE_implication(es_staging, harmony_symbols)
DEFINE_implication(es_staging, harmony_collections)

// Flags for experimental implementation features.
DEFINE_bool(packed_arrays, true, "optimizes arrays that have no holes")
DEFINE_bool(smi_only_arrays, true, "tracks arrays with only smi values")
DEFINE_bool(compiled_keyed_dictionary_loads, true,
            "use optimizing compiler to generate keyed dictionary load stubs")
DEFINE_bool(compiled_keyed_generic_loads, false,
            "use optimizing compiler to generate keyed generic load stubs")
DEFINE_bool(clever_optimizations, true,
            "Optimize object size, Array shift, DOM strings and string +")
// TODO(hpayer): We will remove this flag as soon as we have pretenuring
// support for specific allocation sites.
DEFINE_bool(pretenuring_call_new, false, "pretenure call new")
DEFINE_bool(allocation_site_pretenuring, true,
            "pretenure with allocation sites")
DEFINE_bool(trace_pretenuring, false,
            "trace pretenuring decisions of HAllocate instructions")
DEFINE_bool(trace_pretenuring_statistics, false,
            "trace allocation site pretenuring statistics")
DEFINE_bool(track_fields, true, "track fields with only smi values")
DEFINE_bool(track_double_fields, true, "track fields with double values")
DEFINE_bool(track_heap_object_fields, true, "track fields with heap values")
DEFINE_bool(track_computed_fields, true, "track computed boilerplate fields")
DEFINE_implication(track_double_fields, track_fields)
DEFINE_implication(track_heap_object_fields, track_fields)
DEFINE_implication(track_computed_fields, track_fields)
DEFINE_bool(track_field_types, true, "track field types")
DEFINE_implication(track_field_types, track_fields)
DEFINE_implication(track_field_types, track_heap_object_fields)
DEFINE_bool(smi_binop, true, "support smi representation in binary operations")

// Flags for optimization types.
DEFINE_bool(optimize_for_size, false,
            "Enables optimizations which favor memory size over execution "
            "speed.")

// Flags for data representation optimizations
DEFINE_bool(unbox_double_arrays, true, "automatically unbox arrays of doubles")
DEFINE_bool(string_slices, true, "use string slices")

// Flags for Crankshaft.
DEFINE_bool(crankshaft, true, "use crankshaft")
DEFINE_string(hydrogen_filter, "*", "optimization filter")
DEFINE_bool(use_gvn, true, "use hydrogen global value numbering")
DEFINE_int(gvn_iterations, 3, "maximum number of GVN fix-point iterations")
DEFINE_bool(use_canonicalizing, true, "use hydrogen instruction canonicalizing")
DEFINE_bool(use_inlining, true, "use function inlining")
DEFINE_bool(use_escape_analysis, true, "use hydrogen escape analysis")
DEFINE_bool(use_allocation_folding, true, "use allocation folding")
DEFINE_bool(use_local_allocation_folding, false, "only fold in basic blocks")
DEFINE_bool(use_write_barrier_elimination, true,
            "eliminate write barriers targeting allocations in optimized code")
DEFINE_int(max_inlining_levels, 5, "maximum number of inlining levels")
DEFINE_int(max_inlined_source_size, 600,
           "maximum source size in bytes considered for a single inlining")
DEFINE_int(max_inlined_nodes, 196,
           "maximum number of AST nodes considered for a single inlining")
DEFINE_int(max_inlined_nodes_cumulative, 400,
           "maximum cumulative number of AST nodes considered for inlining")
DEFINE_bool(loop_invariant_code_motion, true, "loop invariant code motion")
DEFINE_bool(fast_math, true, "faster (but maybe less accurate) math functions")
DEFINE_bool(collect_megamorphic_maps_from_stub_cache, true,
            "crankshaft harvests type feedback from stub cache")
DEFINE_bool(hydrogen_stats, false, "print statistics for hydrogen")
DEFINE_bool(trace_check_elimination, false, "trace check elimination phase")
DEFINE_bool(trace_hydrogen, false, "trace generated hydrogen to file")
DEFINE_string(trace_hydrogen_filter, "*", "hydrogen tracing filter")
DEFINE_bool(trace_hydrogen_stubs, false, "trace generated hydrogen for stubs")
DEFINE_string(trace_hydrogen_file, NULL, "trace hydrogen to given file name")
DEFINE_string(trace_phase, "HLZ", "trace generated IR for specified phases")
DEFINE_bool(trace_inlining, false, "trace inlining decisions")
DEFINE_bool(trace_load_elimination, false, "trace load elimination")
DEFINE_bool(trace_store_elimination, false, "trace store elimination")
DEFINE_bool(trace_alloc, false, "trace register allocator")
DEFINE_bool(trace_all_uses, false, "trace all use positions")
DEFINE_bool(trace_range, false, "trace range analysis")
DEFINE_bool(trace_gvn, false, "trace global value numbering")
DEFINE_bool(trace_representation, false, "trace representation types")
DEFINE_bool(trace_removable_simulates, false, "trace removable simulates")
DEFINE_bool(trace_escape_analysis, false, "trace hydrogen escape analysis")
DEFINE_bool(trace_allocation_folding, false, "trace allocation folding")
DEFINE_bool(trace_track_allocation_sites, false,
            "trace the tracking of allocation sites")
DEFINE_bool(trace_migration, false, "trace object migration")
DEFINE_bool(trace_generalization, false, "trace map generalization")
DEFINE_bool(stress_pointer_maps, false, "pointer map for every instruction")
DEFINE_bool(stress_environments, false, "environment for every instruction")
DEFINE_int(deopt_every_n_times, 0,
           "deoptimize every n times a deopt point is passed")
DEFINE_int(deopt_every_n_garbage_collections, 0,
           "deoptimize every n garbage collections")
DEFINE_bool(print_deopt_stress, false, "print number of possible deopt points")
DEFINE_bool(trap_on_deopt, false, "put a break point before deoptimizing")
DEFINE_bool(trap_on_stub_deopt, false,
            "put a break point before deoptimizing a stub")
DEFINE_bool(deoptimize_uncommon_cases, true, "deoptimize uncommon cases")
DEFINE_bool(polymorphic_inlining, true, "polymorphic inlining")
DEFINE_bool(use_osr, true, "use on-stack replacement")
DEFINE_bool(array_bounds_checks_elimination, true,
            "perform array bounds checks elimination")
DEFINE_bool(trace_bce, false, "trace array bounds check elimination")
DEFINE_bool(array_bounds_checks_hoisting, false,
            "perform array bounds checks hoisting")
DEFINE_bool(array_index_dehoisting, true,
            "perform array index dehoisting")
DEFINE_bool(analyze_environment_liveness, true,
            "analyze liveness of environment slots and zap dead values")
DEFINE_bool(load_elimination, true, "use load elimination")
DEFINE_bool(check_elimination, true, "use check elimination")
DEFINE_bool(store_elimination, false, "use store elimination")
DEFINE_bool(dead_code_elimination, true, "use dead code elimination")
DEFINE_bool(fold_constants, true, "use constant folding")
DEFINE_bool(trace_dead_code_elimination, false, "trace dead code elimination")
DEFINE_bool(unreachable_code_elimination, true, "eliminate unreachable code")
DEFINE_bool(trace_osr, false, "trace on-stack replacement")
DEFINE_int(stress_runs, 0, "number of stress runs")
DEFINE_bool(optimize_closures, true, "optimize closures")
DEFINE_bool(lookup_sample_by_shared, true,
            "when picking a function to optimize, watch for shared function "
            "info, not JSFunction itself")
DEFINE_bool(cache_optimized_code, true,
            "cache optimized code for closures")
DEFINE_bool(flush_optimized_code_cache, true,
            "flushes the cache of optimized code for closures on every GC")
DEFINE_bool(inline_construct, true, "inline constructor calls")
DEFINE_bool(inline_arguments, true, "inline functions with arguments object")
DEFINE_bool(inline_accessors, true, "inline JavaScript accessors")
DEFINE_int(escape_analysis_iterations, 2,
           "maximum number of escape analysis fix-point iterations")

DEFINE_bool(optimize_for_in, true,
            "optimize functions containing for-in loops")
DEFINE_bool(opt_safe_uint32_operations, true,
            "allow uint32 values on optimize frames if they are used only in "
            "safe operations")

DEFINE_bool(concurrent_recompilation, true,
            "optimizing hot functions asynchronously on a separate thread")
DEFINE_bool(trace_concurrent_recompilation, false,
            "track concurrent recompilation")
DEFINE_int(concurrent_recompilation_queue_length, 8,
           "the length of the concurrent compilation queue")
DEFINE_int(concurrent_recompilation_delay, 0,
           "artificial compilation delay in ms")
DEFINE_bool(block_concurrent_recompilation, false,
            "block queued jobs until released")
DEFINE_bool(concurrent_osr, true,
            "concurrent on-stack replacement")
DEFINE_implication(concurrent_osr, concurrent_recompilation)

DEFINE_bool(omit_map_checks_for_leaf_maps, true,
            "do not emit check maps for constant values that have a leaf map, "
            "deoptimize the optimized code if the layout of the maps changes.")

DEFINE_int(typed_array_max_size_in_heap, 64,
    "threshold for in-heap typed array")

// Profiler flags.
DEFINE_int(frame_count, 1, "number of stack frames inspected by the profiler")
           // 0x1800 fits in the immediate field of an ARM instruction.
DEFINE_int(interrupt_budget, 0x1800,
           "execution budget before interrupt is triggered")
DEFINE_int(type_info_threshold, 25,
           "percentage of ICs that must have type info to allow optimization")
DEFINE_int(self_opt_count, 130, "call count before self-optimization")

DEFINE_bool(trace_opt_verbose, false, "extra verbose compilation tracing")
DEFINE_implication(trace_opt_verbose, trace_opt)

// assembler-ia32.cc / assembler-arm.cc / assembler-x64.cc
DEFINE_bool(debug_code, false,
            "generate extra code (assertions) for debugging")
DEFINE_bool(code_comments, false, "emit comments in code disassembly")
DEFINE_bool(enable_sse3, true,
            "enable use of SSE3 instructions if available")
DEFINE_bool(enable_sse4_1, true,
            "enable use of SSE4.1 instructions if available")
DEFINE_bool(enable_sahf, true,
            "enable use of SAHF instruction if available (X64 only)")
DEFINE_bool(enable_vfp3, ENABLE_VFP3_DEFAULT,
            "enable use of VFP3 instructions if available")
DEFINE_bool(enable_armv7, ENABLE_ARMV7_DEFAULT,
            "enable use of ARMv7 instructions if available (ARM only)")
DEFINE_bool(enable_neon, ENABLE_NEON_DEFAULT,
            "enable use of NEON instructions if available (ARM only)")
DEFINE_bool(enable_sudiv, true,
            "enable use of SDIV and UDIV instructions if available (ARM only)")
DEFINE_bool(enable_mls, true,
            "enable use of MLS instructions if available (ARM only)")
DEFINE_bool(enable_movw_movt, false,
            "enable loading 32-bit constant by means of movw/movt "
            "instruction pairs (ARM only)")
DEFINE_bool(enable_unaligned_accesses, true,
            "enable unaligned accesses for ARMv7 (ARM only)")
DEFINE_bool(enable_32dregs, ENABLE_32DREGS_DEFAULT,
            "enable use of d16-d31 registers on ARM - this requires VFP3")
DEFINE_bool(enable_vldr_imm, false,
            "enable use of constant pools for double immediate (ARM only)")
DEFINE_bool(force_long_branches, false,
            "force all emitted branches to be in long mode (MIPS only)")

// cpu-arm64.cc
DEFINE_bool(enable_always_align_csp, true,
            "enable alignment of csp to 16 bytes on platforms which prefer "
            "the register to always be aligned (ARM64 only)")

// bootstrapper.cc
DEFINE_string(expose_natives_as, NULL, "expose natives in global object")
DEFINE_string(expose_debug_as, NULL, "expose debug in global object")
DEFINE_bool(expose_free_buffer, false, "expose freeBuffer extension")
DEFINE_bool(expose_gc, false, "expose gc extension")
DEFINE_string(expose_gc_as, NULL,
              "expose gc extension under the specified name")
DEFINE_implication(expose_gc_as, expose_gc)
DEFINE_bool(expose_externalize_string, false,
            "expose externalize string extension")
DEFINE_bool(expose_trigger_failure, false, "expose trigger-failure extension")
DEFINE_int(stack_trace_limit, 10, "number of stack frames to capture")
DEFINE_bool(builtins_in_stack_traces, false,
            "show built-in functions in stack traces")
DEFINE_bool(disable_native_files, false, "disable builtin natives files")

// builtins-ia32.cc
DEFINE_bool(inline_new, true, "use fast inline allocation")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_bool(trace_codegen, false,
            "print name of functions for which code is generated")
DEFINE_bool(trace, false, "trace function calls")
DEFINE_bool(mask_constants_with_cookie, true,
            "use random jit cookie to mask large constants")

// codegen.cc
DEFINE_bool(lazy, true, "use lazy compilation")
DEFINE_bool(trace_opt, false, "trace lazy optimization")
DEFINE_bool(trace_opt_stats, false, "trace lazy optimization statistics")
DEFINE_bool(opt, true, "use adaptive optimizations")
DEFINE_bool(always_opt, false, "always try to optimize functions")
DEFINE_bool(always_osr, false, "always try to OSR functions")
DEFINE_bool(prepare_always_opt, false, "prepare for turning on always opt")
DEFINE_bool(trace_deopt, false, "trace optimize function deoptimization")
DEFINE_bool(trace_stub_failures, false,
            "trace deoptimization of generated code stubs")

// compiler.cc
DEFINE_int(min_preparse_length, 1024,
           "minimum length for automatic enable preparsing")
DEFINE_bool(always_full_compiler, false,
            "try to use the dedicated run-once backend for all code")
DEFINE_int(max_opt_count, 10,
           "maximum number of optimization attempts before giving up.")

// compilation-cache.cc
DEFINE_bool(compilation_cache, true, "enable compilation cache")

DEFINE_bool(cache_prototype_transitions, true, "cache prototype transitions")

// cpu-profiler.cc
DEFINE_int(cpu_profiler_sampling_interval, 1000,
           "CPU profiler sampling interval in microseconds")

// debug.cc
DEFINE_bool(trace_debug_json, false, "trace debugging JSON request/response")
DEFINE_bool(trace_js_array_abuse, false,
            "trace out-of-bounds accesses to JS arrays")
DEFINE_bool(trace_external_array_abuse, false,
            "trace out-of-bounds-accesses to external arrays")
DEFINE_bool(trace_array_abuse, false,
            "trace out-of-bounds accesses to all arrays")
DEFINE_implication(trace_array_abuse, trace_js_array_abuse)
DEFINE_implication(trace_array_abuse, trace_external_array_abuse)
DEFINE_bool(enable_liveedit, true, "enable liveedit experimental feature")
DEFINE_bool(hard_abort, true, "abort by crashing")

// execution.cc
// Slightly less than 1MB on 64-bit, since Windows' default stack size for
// the main execution thread is 1MB for both 32 and 64-bit.
DEFINE_int(stack_size, kPointerSize * 123,
           "default size of stack region v8 is allowed to use (in kBytes)")

// frames.cc
DEFINE_int(max_stack_trace_source_length, 300,
           "maximum length of function source code printed in a stack trace.")

// full-codegen.cc
DEFINE_bool(always_inline_smi_code, false,
            "always inline smi code in non-opt code")

// heap.cc
DEFINE_int(min_semi_space_size, 0,
    "min size of a semi-space (in MBytes), the new space consists of two"
    "semi-spaces")
DEFINE_int(max_semi_space_size, 0,
    "max size of a semi-space (in MBytes), the new space consists of two"
    "semi-spaces")
DEFINE_int(max_old_space_size, 0, "max size of the old space (in Mbytes)")
DEFINE_int(max_executable_size, 0, "max size of executable memory (in Mbytes)")
DEFINE_bool(gc_global, false, "always perform global GCs")
DEFINE_int(gc_interval, -1, "garbage collect after <n> allocations")
DEFINE_bool(trace_gc, false,
            "print one trace line following each garbage collection")
DEFINE_bool(trace_gc_nvp, false,
            "print one detailed trace line in name=value format "
            "after each garbage collection")
DEFINE_bool(trace_gc_ignore_scavenger, false,
            "do not print trace line after scavenger collection")
DEFINE_bool(print_cumulative_gc_stat, false,
            "print cumulative GC statistics in name=value format on exit")
DEFINE_bool(print_max_heap_committed, false,
            "print statistics of the maximum memory committed for the heap "
            "in name=value format on exit")
DEFINE_bool(trace_gc_verbose, false,
            "print more details following each garbage collection")
DEFINE_bool(trace_fragmentation, false,
            "report fragmentation for old pointer and data pages")
DEFINE_bool(collect_maps, true,
            "garbage collect maps from which no objects can be reached")
DEFINE_bool(weak_embedded_maps_in_ic, true,
            "make maps embedded in inline cache stubs")
DEFINE_bool(weak_embedded_maps_in_optimized_code, true,
            "make maps embedded in optimized code weak")
DEFINE_bool(weak_embedded_objects_in_optimized_code, true,
            "make objects embedded in optimized code weak")
DEFINE_bool(flush_code, true,
            "flush code that we expect not to use again (during full gc)")
DEFINE_bool(flush_code_incrementally, true,
            "flush code that we expect not to use again (incrementally)")
DEFINE_bool(trace_code_flushing, false, "trace code flushing progress")
DEFINE_bool(age_code, true,
            "track un-executed functions to age code and flush only "
            "old code (required for code flushing)")
DEFINE_bool(incremental_marking, true, "use incremental marking")
DEFINE_bool(incremental_marking_steps, true, "do incremental marking steps")
DEFINE_bool(trace_incremental_marking, false,
            "trace progress of the incremental marking")
DEFINE_bool(track_gc_object_stats, false,
            "track object counts and memory usage")
DEFINE_bool(parallel_sweeping, false, "enable parallel sweeping")
DEFINE_bool(concurrent_sweeping, true, "enable concurrent sweeping")
DEFINE_int(sweeper_threads, 0,
           "number of parallel and concurrent sweeping threads")
DEFINE_bool(job_based_sweeping, false, "enable job based sweeping")
#ifdef VERIFY_HEAP
DEFINE_bool(verify_heap, false, "verify heap pointers before and after GC")
#endif


// heap-snapshot-generator.cc
DEFINE_bool(heap_profiler_trace_objects, false,
            "Dump heap object allocations/movements/size_updates")


// v8.cc
DEFINE_bool(use_idle_notification, true,
            "Use idle notification to reduce memory footprint.")
// ic.cc
DEFINE_bool(use_ic, true, "use inline caching")

// macro-assembler-ia32.cc
DEFINE_bool(native_code_counters, false,
            "generate extra code for manipulating stats counters")

// mark-compact.cc
DEFINE_bool(always_compact, false, "Perform compaction on every full GC")
DEFINE_bool(never_compact, false,
            "Never perform compaction on full GC - testing only")
DEFINE_bool(compact_code_space, true,
            "Compact code space on full non-incremental collections")
DEFINE_bool(incremental_code_compaction, true,
            "Compact code space on full incremental collections")
DEFINE_bool(cleanup_code_caches_at_gc, true,
            "Flush inline caches prior to mark compact collection and "
            "flush code caches in maps during mark compact cycle.")
DEFINE_bool(use_marking_progress_bar, true,
            "Use a progress bar to scan large objects in increments when "
            "incremental marking is active.")
DEFINE_bool(zap_code_space, true,
            "Zap free memory in code space with 0xCC while sweeping.")
DEFINE_int(random_seed, 0,
           "Default seed for initializing random generator "
           "(0, the default, means to use system random).")

// objects.cc
DEFINE_bool(use_verbose_printer, true, "allows verbose printing")

// parser.cc
DEFINE_bool(allow_natives_syntax, false, "allow natives syntax")
DEFINE_bool(trace_parse, false, "trace parsing and preparsing")

// simulator-arm.cc, simulator-arm64.cc and simulator-mips.cc
DEFINE_bool(trace_sim, false, "Trace simulator execution")
DEFINE_bool(debug_sim, false, "Enable debugging the simulator")
DEFINE_bool(check_icache, false,
            "Check icache flushes in ARM and MIPS simulator")
DEFINE_int(stop_sim_at, 0, "Simulator stop after x number of instructions")
#ifdef V8_TARGET_ARCH_ARM64
DEFINE_int(sim_stack_alignment, 16,
           "Stack alignment in bytes in simulator. This must be a power of two "
           "and it must be at least 16. 16 is default.")
#else
DEFINE_int(sim_stack_alignment, 8,
           "Stack alingment in bytes in simulator (4 or 8, 8 is default)")
#endif
DEFINE_int(sim_stack_size, 2 * MB / KB,
           "Stack size of the ARM64 simulator in kBytes (default is 2 MB)")
DEFINE_bool(log_regs_modified, true,
            "When logging register values, only print modified registers.")
DEFINE_bool(log_colour, true,
            "When logging, try to use coloured output.")
DEFINE_bool(ignore_asm_unimplemented_break, false,
            "Don't break for ASM_UNIMPLEMENTED_BREAK macros.")
DEFINE_bool(trace_sim_messages, false,
            "Trace simulator debug messages. Implied by --trace-sim.")

// isolate.cc
DEFINE_bool(stack_trace_on_illegal, false,
            "print stack trace when an illegal exception is thrown")
DEFINE_bool(abort_on_uncaught_exception, false,
            "abort program (dump core) when an uncaught exception is thrown")
DEFINE_bool(randomize_hashes, true,
            "randomize hashes to avoid predictable hash collisions "
            "(with snapshots this option cannot override the baked-in seed)")
DEFINE_int(hash_seed, 0,
           "Fixed seed to use to hash property keys (0 means random)"
           "(with snapshots this option cannot override the baked-in seed)")

// snapshot-common.cc
DEFINE_bool(profile_deserialization, false,
            "Print the time it takes to deserialize the snapshot.")

// Regexp
DEFINE_bool(regexp_optimization, true, "generate optimized regexp code")

// Testing flags test/cctest/test-{flags,api,serialization}.cc
DEFINE_bool(testing_bool_flag, true, "testing_bool_flag")
DEFINE_maybe_bool(testing_maybe_bool_flag, "testing_maybe_bool_flag")
DEFINE_int(testing_int_flag, 13, "testing_int_flag")
DEFINE_float(testing_float_flag, 2.5, "float-flag")
DEFINE_string(testing_string_flag, "Hello, world!", "string-flag")
DEFINE_int(testing_prng_seed, 42, "Seed used for threading test randomness")
#ifdef _WIN32
DEFINE_string(testing_serialization_file, "C:\\Windows\\Temp\\serdes",
              "file in which to testing_serialize heap")
#else
DEFINE_string(testing_serialization_file, "/tmp/serdes",
              "file in which to serialize heap")
#endif

// mksnapshot.cc
DEFINE_string(extra_code, NULL, "A filename with extra code to be included in"
                                " the snapshot (mksnapshot only)")
DEFINE_string(raw_file, NULL, "A file to write the raw snapshot bytes to. "
                              "(mksnapshot only)")
DEFINE_string(raw_context_file, NULL, "A file to write the raw context "
                                      "snapshot bytes to. (mksnapshot only)")
DEFINE_bool(omit, false, "Omit raw snapshot bytes in generated code. "
                         "(mksnapshot only)")

// code-stubs-hydrogen.cc
DEFINE_bool(profile_hydrogen_code_stub_compilation, false,
            "Print the time it takes to lazily compile hydrogen code stubs.")

DEFINE_bool(predictable, false, "enable predictable mode")
DEFINE_neg_implication(predictable, concurrent_recompilation)
DEFINE_neg_implication(predictable, concurrent_osr)
DEFINE_neg_implication(predictable, concurrent_sweeping)
DEFINE_neg_implication(predictable, parallel_sweeping)


//
// Dev shell flags
//

DEFINE_bool(help, false, "Print usage message, including flags, on console")
DEFINE_bool(dump_counters, false, "Dump counters on exit")

DEFINE_bool(debugger, false, "Enable JavaScript debugger")

DEFINE_string(map_counters, "", "Map counters to a file")
DEFINE_args(js_arguments,
            "Pass all remaining arguments to the script. Alias for \"--\".")

//
// GDB JIT integration flags.
//

DEFINE_bool(gdbjit, false, "enable GDBJIT interface (disables compacting GC)")
DEFINE_bool(gdbjit_full, false, "enable GDBJIT interface for all code objects")
DEFINE_bool(gdbjit_dump, false, "dump elf objects with debug info to disk")
DEFINE_string(gdbjit_dump_filter, "",
              "dump only objects containing this substring")

// mark-compact.cc
DEFINE_bool(force_marking_deque_overflows, false,
            "force overflows of marking deque by reducing it's size "
            "to 64 words")

DEFINE_bool(stress_compaction, false,
            "stress the GC compactor to flush out bugs (implies "
            "--force_marking_deque_overflows)")

//
// Debug only flags
//
#undef FLAG
#ifdef DEBUG
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

// checks.cc
#ifdef ENABLE_SLOW_ASSERTS
DEFINE_bool(enable_slow_asserts, false,
            "enable asserts that are slow to execute")
#endif

// codegen-ia32.cc / codegen-arm.cc / macro-assembler-*.cc
DEFINE_bool(print_source, false, "pretty print source code")
DEFINE_bool(print_builtin_source, false,
            "pretty print source code for builtins")
DEFINE_bool(print_ast, false, "print source AST")
DEFINE_bool(print_builtin_ast, false, "print source AST for builtins")
DEFINE_string(stop_at, "", "function name where to insert a breakpoint")
DEFINE_bool(trap_on_abort, false, "replace aborts by breakpoints")

// compiler.cc
DEFINE_bool(print_builtin_scopes, false, "print scopes for builtins")
DEFINE_bool(print_scopes, false, "print scopes")

// contexts.cc
DEFINE_bool(trace_contexts, false, "trace contexts operations")

// heap.cc
DEFINE_bool(gc_verbose, false, "print stuff during garbage collection")
DEFINE_bool(heap_stats, false, "report heap statistics before and after GC")
DEFINE_bool(code_stats, false, "report code statistics after GC")
DEFINE_bool(verify_native_context_separation, false,
            "verify that code holds on to at most one native context after GC")
DEFINE_bool(print_handles, false, "report handles after GC")
DEFINE_bool(print_global_handles, false, "report global handles after GC")

// ic.cc
DEFINE_bool(trace_ic, false, "trace inline cache state transitions")

// interface.cc
DEFINE_bool(print_interfaces, false, "print interfaces")
DEFINE_bool(print_interface_details, false, "print interface inference details")
DEFINE_int(print_interface_depth, 5, "depth for printing interfaces")

// objects.cc
DEFINE_bool(trace_normalization, false,
            "prints when objects are turned into dictionaries.")

// runtime.cc
DEFINE_bool(trace_lazy, false, "trace lazy compilation")

// spaces.cc
DEFINE_bool(collect_heap_spill_statistics, false,
            "report heap spill statistics along with heap_stats "
            "(requires heap_stats)")

DEFINE_bool(trace_isolates, false, "trace isolate state changes")

// Regexp
DEFINE_bool(regexp_possessive_quantifier, false,
            "enable possessive quantifier syntax for testing")
DEFINE_bool(trace_regexp_bytecodes, false, "trace regexp bytecode execution")
DEFINE_bool(trace_regexp_assembler, false,
            "trace regexp macro assembler calls.")

//
// Logging and profiling flags
//
#undef FLAG
#define FLAG FLAG_FULL

// log.cc
DEFINE_bool(log, false,
            "Minimal logging (no API, code, GC, suspect, or handles samples).")
DEFINE_bool(log_all, false, "Log all events to the log file.")
DEFINE_bool(log_api, false, "Log API events to the log file.")
DEFINE_bool(log_code, false,
            "Log code events to the log file without profiling.")
DEFINE_bool(log_gc, false,
            "Log heap samples on garbage collection for the hp2ps tool.")
DEFINE_bool(log_handles, false, "Log global handle events.")
DEFINE_bool(log_snapshot_positions, false,
            "log positions of (de)serialized objects in the snapshot.")
DEFINE_bool(log_suspect, false, "Log suspect operations.")
DEFINE_bool(prof, false,
            "Log statistical profiling information (implies --log-code).")
DEFINE_bool(prof_browser_mode, true,
            "Used with --prof, turns on browser-compatible mode for profiling.")
DEFINE_bool(log_regexp, false, "Log regular expression execution.")
DEFINE_string(logfile, "v8.log", "Specify the name of the log file.")
DEFINE_bool(logfile_per_isolate, true, "Separate log files for each isolate.")
DEFINE_bool(ll_prof, false, "Enable low-level linux profiler.")
DEFINE_bool(perf_basic_prof, false,
            "Enable perf linux profiler (basic support).")
DEFINE_bool(perf_jit_prof, false,
            "Enable perf linux profiler (experimental annotate support).")
DEFINE_string(gc_fake_mmap, "/tmp/__v8_gc__",
              "Specify the name of the file for fake gc mmap used in ll_prof")
DEFINE_bool(log_internal_timer_events, false, "Time internal events.")
DEFINE_bool(log_timer_events, false,
            "Time events including external callbacks.")
DEFINE_implication(log_timer_events, log_internal_timer_events)
DEFINE_implication(log_internal_timer_events, prof)
DEFINE_bool(log_instruction_stats, false, "Log AArch64 instruction statistics.")
DEFINE_string(log_instruction_file, "arm64_inst.csv",
              "AArch64 instruction statistics log file.")
DEFINE_int(log_instruction_period, 1 << 22,
           "AArch64 instruction statistics logging period.")

DEFINE_bool(redirect_code_traces, false,
            "output deopt information and disassembly into file "
            "code-<pid>-<isolate id>.asm")
DEFINE_string(redirect_code_traces_to, NULL,
            "output deopt information and disassembly into the given file")

DEFINE_bool(hydrogen_track_positions, false,
            "track source code positions when building IR")

//
// Disassembler only flags
//
#undef FLAG
#ifdef ENABLE_DISASSEMBLER
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

// elements.cc
DEFINE_bool(trace_elements_transitions, false, "trace elements transitions")

DEFINE_bool(trace_creation_allocation_sites, false,
            "trace the creation of allocation sites")

// code-stubs.cc
DEFINE_bool(print_code_stubs, false, "print code stubs")
DEFINE_bool(test_secondary_stub_cache, false,
            "test secondary stub cache by disabling the primary one")

DEFINE_bool(test_primary_stub_cache, false,
            "test primary stub cache by disabling the secondary one")


// codegen-ia32.cc / codegen-arm.cc
DEFINE_bool(print_code, false, "print generated code")
DEFINE_bool(print_opt_code, false, "print optimized code")
DEFINE_bool(print_unopt_code, false, "print unoptimized code before "
            "printing optimized code based on it")
DEFINE_bool(print_code_verbose, false, "print more information for code")
DEFINE_bool(print_builtin_code, false, "print generated code for builtins")

#ifdef ENABLE_DISASSEMBLER
DEFINE_bool(sodium, false, "print generated code output suitable for use with "
            "the Sodium code viewer")

DEFINE_implication(sodium, print_code_stubs)
DEFINE_implication(sodium, print_code)
DEFINE_implication(sodium, print_opt_code)
DEFINE_implication(sodium, hydrogen_track_positions)
DEFINE_implication(sodium, code_comments)

DEFINE_bool(print_all_code, false, "enable all flags related to printing code")
DEFINE_implication(print_all_code, print_code)
DEFINE_implication(print_all_code, print_opt_code)
DEFINE_implication(print_all_code, print_unopt_code)
DEFINE_implication(print_all_code, print_code_verbose)
DEFINE_implication(print_all_code, print_builtin_code)
DEFINE_implication(print_all_code, print_code_stubs)
DEFINE_implication(print_all_code, code_comments)
#ifdef DEBUG
DEFINE_implication(print_all_code, trace_codegen)
#endif
#endif

//
// Read-only flags
//
#undef FLAG
#define FLAG FLAG_READONLY

// assembler-arm.h
DEFINE_bool(enable_ool_constant_pool, V8_OOL_CONSTANT_POOL,
            "enable use of out-of-line constant pools (ARM only)")

// Cleanup...
#undef FLAG_FULL
#undef FLAG_READONLY
#undef FLAG
#undef FLAG_ALIAS

#undef DEFINE_bool
#undef DEFINE_maybe_bool
#undef DEFINE_int
#undef DEFINE_string
#undef DEFINE_float
#undef DEFINE_args
#undef DEFINE_implication
#undef DEFINE_neg_implication
#undef DEFINE_ALIAS_bool
#undef DEFINE_ALIAS_int
#undef DEFINE_ALIAS_string
#undef DEFINE_ALIAS_float
#undef DEFINE_ALIAS_args

#undef FLAG_MODE_DECLARE
#undef FLAG_MODE_DEFINE
#undef FLAG_MODE_DEFINE_DEFAULTS
#undef FLAG_MODE_META
#undef FLAG_MODE_DEFINE_IMPLICATIONS

#undef COMMA
