#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "amd64.h"

// A bit set capable of storing values between 0 and N-1.  Operations
// marked "atomic" are safe to execute concurrently.  Read-only
// operations are also safe to execute concurrently, but may not
// return a consistent view of the bitset.
template<std::size_t N>
class bitset
{
  enum {
    BITS_PER_WORD = sizeof(uint64_t) * 8,
    NWORDS = (N + BITS_PER_WORD - 1) / BITS_PER_WORD
  };

  uint64_t words[NWORDS];

public:
  // Construct an empty bit set.
  constexpr bitset() : words{} { }

  // If @c val is true, add pos to the bit set.  If @c val is false,
  // remove pos from the bit set.
  bitset &set(std::size_t pos, bool val = true) noexcept
  {
    if (val) {
      words[pos / BITS_PER_WORD] |= (1ull << (pos % BITS_PER_WORD));
      return *this;
    } else {
      return reset(pos);
    }
  }

  // Remove @c pos from the bit set.
  bitset &reset(std::size_t pos) noexcept
  {
    words[pos / BITS_PER_WORD] &= ~(1ull << (pos % BITS_PER_WORD));
    return *this;
  }

  // If @c val is true, add pos to the bit set.  If @c val is false,
  // remove pos from the bit set.
  bitset &atomic_set(std::size_t pos, bool val = true) noexcept
  {
    if (val) {
      locked_set_bit(&words[pos / BITS_PER_WORD], pos % BITS_PER_WORD);
      return *this;
    } else {
      return reset(pos);
    }
  }

  // Remove @c pos from the bit set.
  bitset &atomic_reset(std::size_t pos) noexcept
  {
    locked_reset_bit(&words[pos / BITS_PER_WORD], pos % BITS_PER_WORD);
    return *this;
  }

  // Remove all values from the bit set.
  bitset &reset() noexcept
  {
    memset(words, 0, sizeof words);
    return *this;
  }

  // Update this bit set to the intersection of this and @c rhs.
  bitset& operator&=(const bitset& rhs) noexcept
  {
    for (size_t i = 0; i < NWORDS; ++i)
      words[i] &= rhs.words[i];
    return *this;
  }

  // Update this bit set to the union of this and @c rhs.
  bitset& operator|=(const bitset& rhs) noexcept
  {
    for (size_t i = 0; i < NWORDS; ++i)
      words[i] |= rhs.words[i];
    return *this;
  }

  // Test if bit @c pos is set.
  bool operator[](size_t pos) const
  {
    return words[pos / BITS_PER_WORD] & (1ull << (pos % BITS_PER_WORD));
  }

  // Return true if no bits are set.
  bool none() const noexcept
  {
    uint64_t merge = 0;
    for (size_t i = 0; i < NWORDS; ++i)
      merge |= words[i];
    return merge == 0;
  }

  // Return true if any bits are set.
  bool any() const noexcept
  {
    return !none();
  }

  // Return the number of set bits.
  std::size_t count() const noexcept
  {
    std::size_t res = 0;
    for (size_t i = 0; i < NWORDS; ++i)
      // We would use the GCC intrinsic, but that requires enabling
      // SSE4.2, and we don't want GCC to use SSE otherwise.
      res += popcnt64(words[i]);
    return res;
  }

  class iterator
  {
    size_t wordidx;
    uint64_t word;
    const bitset *src;

    friend class bitset;

    iterator(const bitset *src) : wordidx(0), word(0), src(src)
    {
      // Find first non-zero word.  If there are none, word will
      // remain 0 and wordidx will be NWORDS, which is the end
      // iterator.
      for (; wordidx < NWORDS; ++wordidx) {
        if (src->words[wordidx]) {
          word = src->words[wordidx];
          break;
        }
      }
    }

    // Construct the end iterator.
    constexpr iterator() : wordidx(NWORDS), word(0), src(nullptr) { }

  public:
    unsigned operator*() const
    {
      static_assert(sizeof(long long) == sizeof(word),
                    "Size mismatch, wrong ctz builtin");
      return __builtin_ctzll(word) + wordidx * BITS_PER_WORD;
    }

    iterator &operator++()
    {
      // Clear least significant bit
      word &= word - 1;
      if (word == 0)
        if (++wordidx < NWORDS)
          word = src->words[wordidx];
      return *this;
    }

    bool operator==(const iterator &o) const
    {
      return wordidx == o.wordidx && word == o.word;
    }

    bool operator!=(const iterator &o) const
    {
      return !(*this == o);
    }
  };

  // Return an iterator over the set values in the bit set.
  iterator begin() const
  {
    return iterator(this);
  }

  // Return the end iterator.
  iterator end() const
  {
    return iterator();
  }
};
