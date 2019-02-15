#ifndef UINT512_T_H
#define UINT512_T_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numeric>
#include <regex>
#include <vector>

namespace {
#define BYTES_UINT64 (64 / 8)
#define BYTES_UINT128 (128 / 8)
#define BYTES_UINT256 (256 / 8)
#define BYTES_UINT512 (512 / 8)
#define BYTES_UINT1024 (1024 / 8)

template <typename... Args>
std::string stringFormat(const std::string &format, Args... args) {
  size_t size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
  std::unique_ptr<char[]> buf(new char[size]);
  std::snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(),
                     buf.get() + size - 1); // We don't want the '\0' inside
}

std::vector<std::string> split(const std::string &s, const std::string &sep) {
  std::vector<std::string> output;
  std::string::size_type prev_pos = 0, pos = 0;

  if (s.length() == 0) {
    return output;
  }

  while ((pos = s.find(sep, pos)) != std::string::npos) {
    std::string substring(s.substr(prev_pos, pos - prev_pos));
    output.push_back(substring);
    pos += sep.length();
    prev_pos = pos;
  }
  output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word

  return output;
}
} // namespace

namespace op {
void add64(uint64_t &dst, uint64_t value, uint64_t &carry) {
  // 1 + 0xFF + 0xFF = 0xFF, carry = 1
  uint64_t carry_in = carry;
  carry = 0;
  asm("add %3, %0   \n\t"
      "adc $0, %1   \n\t"
      "add %2, %0   \n\t"
      "adc $0, %1   \n\t"
      : "+r"(dst), "+r"(carry)
      : "r"(value), "r"(carry_in)
      : "cc");
  assert(carry == 0 or carry == 1);
}

void add12864(uint64_t &hi, uint64_t &lo, uint64_t value, uint64_t &carry) {
  op::add64(lo, value, carry);
  op::add64(hi, 0, carry);
}

void add128(uint64_t &hi, uint64_t &lo, uint64_t vhi, uint64_t vlo,
            uint64_t &carry) {
  op::add64(lo, vlo, carry);
  op::add64(hi, vhi, carry);
}

uint64_t mul64(uint64_t &dst, uint64_t value) {
  // Unsigned multiply (RDX:RAX ← RAX ∗ r/m64).
  uint64_t spill = 0;
  asm("mul %3" : "=a"(dst), "=d"(spill) : "a"(dst), "r"(value));
  return spill;
}

uint64_t mul12864(uint64_t &hi, uint64_t &lo, uint64_t value) {
  uint64_t spill_lo = op::mul64(lo, value);
  uint64_t spill_hi = op::mul64(hi, value);
  uint64_t carry = 0;
  op::add12864(spill_hi, hi, spill_lo, carry);
  return spill_hi;
}

// We do not implement a mul128 becase this op will result a uint128_t as spill,
// so we put it into class.

} // namespace op

template <typename T, typename SubT> class base {
public:
  base() = delete;
  explicit base(SubT hi, SubT lo) : hi_(hi), lo_(lo) {}

  void parseString(std::string value, size_t typeBytes, size_t subTypeBytes) {
    bool isHex = false, isPositive = true;

    // 0xFFFF_FFFF
    value = std::regex_replace(value, std::regex("_"), "");

    // split 0x and flag
    std::vector<std::string> split0x = split(value, "0x");
    switch (split0x.size()) {
    case 1: {
      isHex = false;
      value = split0x[0];
      if (value[0] == '+' or value[0] == '-') {
        isPositive = (value[0] == '-') ? false : true;
        value = value.replace(value.begin(), value.begin() + 1, "");
      }
      break;
    }
    case 2: {
      isHex = true;
      isPositive = (split0x[0] == "-") ? false : true;
      value = split0x[1];
      break;
    }
    default: { throw std::runtime_error("value is empty"); }
    }

    // check
    for (auto &ch : value) {
      if (isHex) {
        if (ch >= 'a' and ch <= 'f') {
          ch -= 'a' - 'A';
        }
        if ((ch < '0' or ch > 'F') or (ch > '9' and ch < 'A')) {
          throw std::runtime_error("illegal value");
        }
      } else {
        if (ch < '0' or ch > '9') {
          throw std::runtime_error("illegal value");
        }
      }
    }

    // parse string to hex list like [1, 255, 200]
    std::vector<uint32_t> byteList;
    if (isHex) {
      std::reverse(value.begin(), value.end());

      // make aligned
      if (value.length() % 2 == 1) {
        value.push_back('0');
      }

      // [b'A', b'1', b'2'] to [10, 1, 2]
      for (size_t i = 0; i < value.length(); i += 2) {
        uint8_t ch_0 = value[i], ch_1 = value[i + 1];
        ch_0 = (ch_0 >= 'A') ? (10 + ch_0 - 'A') : (ch_0 - '0');
        ch_1 = (ch_1 >= 'A') ? (10 + ch_1 - 'A') : (ch_1 - '0');
        byteList.push_back(16 * ch_1 + ch_0);
      }
    } else {
      byteList.push_back(0);

      for (const auto &ch_x : value) {
        for (auto &hex_x : byteList) {
          hex_x *= 10;
        }

        uint8_t dec_x = ch_x - '0';
        byteList[0] += dec_x;

        for (size_t i = 0; i < byteList.size(); i += 1) {
          if (byteList[i] >= 256) {
            byteList.push_back(0);
            byteList[i + 1] += byteList[i] / 256;
            byteList[i] = byteList[i] % 256;
          }
        }
      }

      while (byteList.size() && byteList[byteList.size() - 1] == 0) {
        byteList.pop_back();
      }
    }

    // byteList now contains reverse-ordered byte value
    if (byteList.size() > typeBytes) {
      throw std::runtime_error("value too much");
    }

    while (byteList.size() < typeBytes) {
      byteList.push_back(0);
    }

    // std::cout << "isHex: " << isHex << std::endl;
    // std::cout << "isPositive: " << isPositive << std::endl;
    // std::cout << value << std::endl;

    // fill data by hex list
    SubT mul = 1;
    for (size_t i = 0; i < typeBytes; i += 1) {
      if (i % subTypeBytes == 0) {
        mul = 1;
      } else {
        mul *= 256;
      }
      size_t id = i / subTypeBytes;
      if (id == 0) {
        this->lo_ += mul * byteList[i];
      } else if (id == 1) {
        this->hi_ += mul * byteList[i];
      } else {
        throw std::runtime_error("should not come here");
      }
      // std::cout << i << " " << this->lo_ << " " << this->hi_ <<
      // std::endl;
    }

    // negative value as complement
    if (not isPositive) {
      this->asComplement();
    }
  }

  virtual SubT getMaxSubT() = 0;

  void asComplement() {
    // complement(0) -> 0
    if (this->hi_ > 0 or this->lo_ > 0) {
      this->hi_ = getMaxSubT() - this->hi_;
      this->lo_ = getMaxSubT() - this->lo_ + 1;
    }
  }

  T toComplement() const {
    T res(this->hi_, this->lo_);
    res.asComplement();
    return res;
  }

  virtual std::string toStringQuick() const = 0;

  std::string toString() const {
    std::string hex = this->toStringQuick();

    // remove duplicate "0"
    size_t fnz = 0;
    for (size_t i = 0; i < hex.size(); i += 1) {
      fnz = i;
      if (hex[i] != '0' and hex[i] != '_') {
        break;
      }
    }
    hex = "0x" + hex.substr(fnz, hex.size() - fnz);
    return hex;
  }

  T &operator=(const T &other) {
    this->hi_ = other.hi_;
    this->lo_ = other.lo_;
    return *this;
  }

  friend bool operator==(const T &left, const T &right) {
    return left.hi_ == right.hi_ and left.lo_ == right.lo_;
  }

  friend bool operator!=(const T &left, const T &right) {
    return not(left == right);
  }

  friend bool operator>(const T &left, const T &right) {
    if (left.hi_ > right.hi_) {
      return true;
    } else if (left.hi_ < right.hi_) {
      return false;
    } else {
      return left.lo_ > right.lo_;
    }
  }

  friend bool operator>=(const T &left, const T &right) {
    return (left > right) or (left == right);
  }

  friend bool operator<(const T &left, const T &right) {
    if (left.hi_ > right.hi_) {
      return false;
    } else if (left.hi_ < right.hi_) {
      return true;
    } else {
      return left.lo_ < right.lo_;
    }
  }

  friend bool operator<=(const T &left, const T &right) {
    return (left < right) or (left == right);
  }

  friend T operator+(const T &left, const T &right) {
    T res = left;
    res += right;
    return res;
  }

  friend T operator-(const T &left, const T &right) {
    T res = left;
    res -= right;
    return res;
  }

  friend T operator*(const T &left, const T &right) {
    T res = left;
    res *= right;
    return res;
  }

  friend std::ostream &operator<<(std::ostream &os, const T &object) {
    return os << object.toString();
  }

protected:
  SubT hi_, lo_;
};

class uint128_t : public base<uint128_t, uint64_t> {
public:
  uint128_t() : base(0, 0) {}
  uint128_t(uint64_t value) : base(0, value) {}
  uint128_t(uint64_t hi, uint64_t lo) : base(hi, lo) {}
  explicit uint128_t(const std::string &value) : base(0, 0) {
    this->parseString(value, BYTES_UINT128, BYTES_UINT64);
  }

  uint64_t getMaxSubT() final {
    static uint64_t MAX_UINT64 = 0xFFFFFFFFFFFFFFFF;
    return MAX_UINT64;
  }

  std::string toStringQuick() const override {
    return stringFormat("%016" PRIX64 "%016" PRIX64 "", this->hi_, this->lo_);
  }

  void addWithCarry(const uint128_t &other, uint64_t &carry) {
    op::add128(this->hi_, this->lo_, other.hi_, other.lo_, carry);
  }

  uint128_t mul128(const uint128_t &other) {
    //      h1 l1
    //      h2 l2
    //    c  b  a
    // f  e  d
    uint64_t b = this->hi_, a = this->lo_;
    uint64_t c = op::mul12864(b, a, other.lo_);
    uint64_t e = this->hi_, d = this->lo_;
    uint64_t f = op::mul12864(e, d, other.hi_);
    this->lo_ = a;
    uint64_t carry = 0;
    op::add128(c, b, e, d, carry);
    this->hi_ = b;
    f += carry;
    return uint128_t(f, c);
  }

  uint128_t &operator+=(const uint128_t &other) {
    uint64_t carry = 0;
    this->addWithCarry(other, carry);
    return *this;
  }

  uint128_t &operator-=(const uint128_t &other) {
    return this->operator+=(other.toComplement());
  }

  uint128_t &operator*=(const uint128_t &other) {
    this->mul128(other);
    return *this;
  }
};

template <typename T, typename SubT> class base_ext : public base<T, SubT> {
public:
  base_ext() = delete;
  explicit base_ext(SubT hi, SubT lo) : base<T, SubT>(hi, lo) {}
};

class uint256_t : public base_ext<uint256_t, uint128_t> {
public:
  uint256_t() : base_ext(0, 0) {}
  uint256_t(uint64_t value) : base_ext(0, value) {}
  uint256_t(const uint128_t &value) : base_ext(0, value) {}
  uint256_t(const uint128_t &hi, const uint128_t &lo) : base_ext(hi, lo) {}
  explicit uint256_t(const std::string &value) : base_ext(0, 0) {
    this->parseString(value, BYTES_UINT256, BYTES_UINT128);
  }

  uint128_t getMaxSubT() final {
    static uint128_t MAX_UINT128 =
        uint128_t(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
    return MAX_UINT128;
  }

  std::string toStringQuick() const override {
    return this->hi_.toStringQuick() + this->lo_.toStringQuick();
  }

  void addWithCarry(const uint256_t &other, uint64_t &carry) {
    this->lo_.addWithCarry(other.lo_, carry);
    this->hi_.addWithCarry(other.hi_, carry);
  }

  uint128_t mul128(const uint128_t &value) {
    //      h  l
    //         v
    //     s_l a
    // s_h  b
    //
    uint128_t spill_lo = this->lo_.mul128(value);
    uint128_t spill_hi = this->hi_.mul128(value);
    uint64_t carry = 0;
    this->hi_.addWithCarry(spill_lo, carry);
    spill_hi.addWithCarry(0, carry);
    return spill_hi;
  }

  uint256_t mul256(const uint256_t &other) {
    //      h1 l1
    //      h2 l2
    //    c  b  a
    // f  e  d
    //
    uint256_t ab(this->hi_, this->lo_);
    uint128_t c = ab.mul128(other.lo_);
    uint256_t ed(this->hi_, this->lo_);
    uint128_t f = ed.mul128(other.hi_);
    uint64_t carry = 0;
    ed.lo_.addWithCarry(ab.hi_, carry);
    ed.hi_.addWithCarry(c, carry);
    f.addWithCarry(0, carry);
    this->lo_ = ab.lo_;
    this->hi_ = ed.lo_;
    return uint256_t(f, ed.hi_);
  }

  uint256_t &operator+=(const uint256_t &other) {
    uint64_t carry = 0;
    this->addWithCarry(other, carry);
    return *this;
  }

  uint256_t &operator-=(const uint256_t &other) {
    return this->operator+=(other.toComplement());
  }

  uint256_t &operator*=(const uint256_t &other) {
    this->mul256(other);
    return *this;
  }
};

class uint512_t : public base_ext<uint512_t, uint256_t> {
public:
  uint512_t() : base_ext(0, 0) {}
  uint512_t(uint64_t value) : base_ext(0, value) {}
  uint512_t(const uint256_t &value) : base_ext(0, value) {}
  uint512_t(const uint256_t &hi, const uint256_t &lo) : base_ext(hi, lo) {}
  explicit uint512_t(const std::string &value) : base_ext(0, 0) {
    this->parseString(value, BYTES_UINT512, BYTES_UINT256);
  }

  uint256_t getMaxSubT() final {
    const uint256_t MAX_UINT256 =
        uint256_t(uint128_t(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
                  uint128_t(0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF));
    return MAX_UINT256;
  }

  std::string toStringQuick() const override {
    return this->hi_.toStringQuick() + this->lo_.toStringQuick();
  }

  void addWithCarry(const uint512_t &other, uint64_t &carry) {
    this->lo_.addWithCarry(other.lo_, carry);
    this->hi_.addWithCarry(other.hi_, carry);
  }

  uint256_t mul256(const uint256_t &value) {
    //      h  l
    //         v
    //     s_l a
    // s_h  b
    //
    uint256_t spill_lo = this->lo_.mul256(value);
    uint256_t spill_hi = this->hi_.mul256(value);
    uint64_t carry = 0;
    this->hi_.addWithCarry(spill_lo, carry);
    spill_hi.addWithCarry(0, carry);
    return spill_hi;
  }

  uint512_t mul512(const uint512_t &other) {
    //      h1 l1
    //      h2 l2
    //    c  b  a
    // f  e  d
    //
    uint512_t ab(this->hi_, this->lo_);
    uint256_t c = ab.mul256(other.lo_);
    uint512_t ed(this->hi_, this->lo_);
    uint256_t f = ed.mul256(other.hi_);
    uint64_t carry = 0;
    ed.lo_.addWithCarry(ab.hi_, carry);
    ed.hi_.addWithCarry(c, carry);
    f.addWithCarry(0, carry);
    this->lo_ = ab.lo_;
    this->hi_ = ed.lo_;
    return uint512_t(f, ed.hi_);
  }

  uint512_t &operator+=(const uint512_t &other) {
    uint64_t carry = 0;
    this->addWithCarry(other, carry);
    return *this;
  }

  uint512_t &operator-=(const uint512_t &other) {
    return this->operator+=(other.toComplement());
  }

  uint512_t &operator*=(const uint512_t &other) {
    this->mul512(other);
    return *this;
  }
};

// class uint1024_t : public base<uint1024_t, uint512_t> {};

#endif
