/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "particle_allocator.hh"

namespace blender::sim {

AttributesAllocator::~AttributesAllocator()
{
  for (std::unique_ptr<AttributesBlock> &block : allocated_blocks_) {
    for (uint i : attributes_info_.index_range()) {
      const fn::CPPType &type = attributes_info_.type_of(i);
      type.destruct_n(block->buffers[i], block->size);
      MEM_freeN(block->buffers[i]);
    }
  }
}

fn::MutableAttributesRef AttributesAllocator::allocate_uninitialized(uint size)
{
  std::unique_ptr<AttributesBlock> block = std::make_unique<AttributesBlock>();
  block->buffers = Array<void *>(attributes_info_.size(), nullptr);
  block->size = size;

  for (uint i : attributes_info_.index_range()) {
    const fn::CPPType &type = attributes_info_.type_of(i);
    void *buffer = MEM_mallocN_aligned(size * type.size(), type.alignment(), AT);
    block->buffers[i] = buffer;
  }

  fn::MutableAttributesRef attributes{attributes_info_, block->buffers, size};

  {
    std::lock_guard lock{mutex_};
    allocated_blocks_.append(std::move(block));
    allocated_attributes_.append(attributes);
    total_allocated_ += size;
  }

  return attributes;
}

fn::MutableAttributesRef ParticleAllocator::allocate(uint size)
{
  const fn::AttributesInfo &info = attributes_allocator_.attributes_info();
  fn::MutableAttributesRef attributes = attributes_allocator_.allocate_uninitialized(size);
  for (uint i : info.index_range()) {
    const fn::CPPType &type = info.type_of(i);
    StringRef name = info.name_of(i);
    if (name == "ID") {
      uint start_id = next_id_.fetch_add(size);
      MutableSpan<int> ids = attributes.get<int>("ID");
      for (uint pindex : IndexRange(size)) {
        ids[pindex] = start_id + pindex;
      }
    }
    else {
      type.fill_uninitialized(info.default_of(i), attributes.get(i).buffer(), size);
    }
  }
  return attributes;
}

}  // namespace blender::sim
