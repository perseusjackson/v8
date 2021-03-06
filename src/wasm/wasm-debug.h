// Copyright 2019 the V8 project authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_DEBUG_H_
#define V8_WASM_WASM_DEBUG_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "include/v8-internal.h"
#include "src/base/iterator.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class JSObject;
class WasmInstanceObject;

namespace wasm {

class DebugInfoImpl;
class NativeModule;

// Side table storing information used to inspect Liftoff frames at runtime.
// This table is only created on demand for debugging, so it is not optimized
// for memory size.
class DebugSideTable {
 public:
  class Entry {
   public:
    struct Constant {
      int index;
      int32_t i32_const;
    };

    Entry(int pc_offset, std::vector<ValueType> stack_types,
          std::vector<Constant> constants)
        : pc_offset_(pc_offset),
          stack_types_(std::move(stack_types)),
          constants_(std::move(constants)) {
      DCHECK(std::is_sorted(constants_.begin(), constants_.end(),
                            ConstantIndexLess{}));
    }

    int pc_offset() const { return pc_offset_; }
    int stack_height() const { return static_cast<int>(stack_types_.size()); }
    ValueType stack_type(int stack_index) const {
      return stack_types_[stack_index];
    }
    // {index} can point to a local or operand stack value.
    bool IsConstant(int index) const {
      return std::binary_search(constants_.begin(), constants_.end(),
                                Constant{index, 0}, ConstantIndexLess{});
    }
    int32_t GetConstant(int index) const {
      DCHECK(IsConstant(index));
      auto it = std::lower_bound(constants_.begin(), constants_.end(),
                                 Constant{index, 0}, ConstantIndexLess{});
      DCHECK_NE(it, constants_.end());
      DCHECK_EQ(it->index, index);
      return it->i32_const;
    }

   private:
    struct ConstantIndexLess {
      bool operator()(const Constant& a, const Constant& b) const {
        return a.index < b.index;
      }
    };

    int pc_offset_;
    std::vector<ValueType> stack_types_;
    std::vector<Constant> constants_;
  };

  // Technically it would be fine to copy this class, but there should not be a
  // reason to do so, hence mark it move only.
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(DebugSideTable);

  explicit DebugSideTable(std::vector<ValueType> local_types,
                          std::vector<Entry> entries)
      : local_types_(std::move(local_types)), entries_(std::move(entries)) {
    DCHECK(
        std::is_sorted(entries_.begin(), entries_.end(), EntryPositionLess{}));
  }

  const Entry* GetEntry(int pc_offset) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(),
                               Entry{pc_offset, {}, {}}, EntryPositionLess{});
    if (it == entries_.end() || it->pc_offset() != pc_offset) return nullptr;
    return &*it;
  }

  auto entries() const {
    return base::make_iterator_range(entries_.begin(), entries_.end());
  }

  size_t num_entries() const { return entries_.size(); }
  int num_locals() const { return static_cast<int>(local_types_.size()); }
  ValueType local_type(int index) const { return local_types_[index]; }

 private:
  struct EntryPositionLess {
    bool operator()(const Entry& a, const Entry& b) const {
      return a.pc_offset() < b.pc_offset();
    }
  };

  std::vector<ValueType> local_types_;
  std::vector<Entry> entries_;
};

// Get the global scope for a given instance. This will contain the wasm memory
// (if the instance has a memory) and the values of all globals.
Handle<JSObject> GetGlobalScopeObject(Handle<WasmInstanceObject>);

// Debug info per NativeModule, created lazily on demand.
// Implementation in {wasm-debug.cc} using PIMPL.
class DebugInfo {
 public:
  explicit DebugInfo(NativeModule*);
  ~DebugInfo();

  Handle<JSObject> GetLocalScopeObject(Isolate*, Address pc, Address fp);

 private:
  std::unique_ptr<DebugInfoImpl> impl_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_DEBUG_H_
