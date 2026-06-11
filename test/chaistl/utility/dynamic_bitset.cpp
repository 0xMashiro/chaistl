// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/dynamic_bitset.hpp>

#include <cstddef>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

static_assert(chaistl::dynamic_bitset<>{}.empty());
static_assert(chaistl::dynamic_bitset<>{8, 0b1010'0101}.count() == 4);
static_assert((chaistl::dynamic_bitset<>{8, 0b0000'1111} << 4).to_ullong() == 0b1111'0000);
static_assert((chaistl::dynamic_bitset<>{8, 0b1111'0000} >> 4).to_ullong() == 0b0000'1111);

TEST(DynamicBitsetTest, ConstructsFromSizeAndInteger) {
  const chaistl::dynamic_bitset<> value(4, 0b1111'1111);

  EXPECT_EQ(value.size(), 4);
  EXPECT_EQ(value.num_blocks(), 1);
  EXPECT_TRUE(value.all());
  EXPECT_EQ(value.count(), 4);
  EXPECT_EQ(value.to_ullong(), 0b1111ull);
  EXPECT_EQ(value.to_string(), "1111");
}

TEST(DynamicBitsetTest, ConstructsFromStringWithCustomDigits) {
  const chaistl::dynamic_bitset<> value(std::string("xxABBAyy"), 2, 4, 'A', 'B');

  EXPECT_EQ(value.size(), 4);
  EXPECT_EQ(value.to_string('A', 'B'), "ABBA");
  EXPECT_EQ(value.to_ullong(), 0b0110ull);
}

TEST(DynamicBitsetTest, StringConstructorValidatesInput) {
  EXPECT_THROW((chaistl::dynamic_bitset<>{std::string("1010"), 5}), std::out_of_range);
  EXPECT_THROW((chaistl::dynamic_bitset<>{std::string("10x0")}), std::invalid_argument);
}

TEST(DynamicBitsetTest, ResizePreservesPrefixAndCanFillNewBits) {
  chaistl::dynamic_bitset<> value(3, 0b101);

  value.resize(70, true);
  EXPECT_EQ(value.size(), 70);
  EXPECT_TRUE(value.test(0));
  EXPECT_FALSE(value.test(1));
  EXPECT_TRUE(value.test(2));
  EXPECT_EQ(value.count(), 69);

  value.resize(2);
  EXPECT_EQ(value.size(), 2);
  EXPECT_TRUE(value.test(0));
  EXPECT_FALSE(value.test(1));
  EXPECT_EQ(value.count(), 1);
}

TEST(DynamicBitsetTest, PushBackPopBackAndReferenceProxyMutateBits) {
  chaistl::dynamic_bitset<> value;

  value.push_back(true);
  value.push_back(false);
  value.push_back(true);
  value[1] = value[0];
  value[2].flip();

  EXPECT_EQ(value.to_string(), "011");
  EXPECT_EQ(value.count(), 2);

  value.pop_back();
  EXPECT_EQ(value.to_string(), "11");
}

TEST(DynamicBitsetTest, ThrowsForCheckedOutOfRangeAccess) {
  chaistl::dynamic_bitset<> value(4);

  EXPECT_THROW((void)value.test(4), std::out_of_range);
  EXPECT_THROW(value.set(4), std::out_of_range);
  EXPECT_THROW(value.reset(4), std::out_of_range);
  EXPECT_THROW(value.flip(4), std::out_of_range);
}

TEST(DynamicBitsetTest, BitwiseOperatorsRequireSameSize) {
  chaistl::dynamic_bitset<> lhs(8, 0b1100);
  chaistl::dynamic_bitset<> rhs(8, 0b1010);
  chaistl::dynamic_bitset<> short_rhs(4, 0b1010);

  EXPECT_EQ((lhs & rhs).to_ullong(), 0b1000ull);
  EXPECT_EQ((lhs | rhs).to_ullong(), 0b1110ull);
  EXPECT_EQ((lhs ^ rhs).to_ullong(), 0b0110ull);
  EXPECT_THROW(lhs &= short_rhs, std::invalid_argument);
}

TEST(DynamicBitsetTest, ShiftOperatorsCrossBlockBoundaries) {
  chaistl::dynamic_bitset<> value(130);
  value.set(0).set(63).set(64);

  const auto shifted_left = value << 65;
  EXPECT_TRUE(shifted_left.test(65));
  EXPECT_TRUE(shifted_left.test(128));
  EXPECT_TRUE(shifted_left.test(129));
  EXPECT_EQ(shifted_left.count(), 3);

  const auto shifted_right = shifted_left >> 65;
  EXPECT_EQ(shifted_right, value);
}

TEST(DynamicBitsetTest, FindFirstAndFindNextReturnNposWhenAbsent) {
  chaistl::dynamic_bitset<> value(130);
  EXPECT_EQ(value.find_first(), chaistl::dynamic_bitset<>::npos);

  value.set(3).set(80).set(129);
  EXPECT_EQ(value.find_first(), 3);
  EXPECT_EQ(value.find_next(3), 80);
  EXPECT_EQ(value.find_next(80), 129);
  EXPECT_EQ(value.find_next(129), chaistl::dynamic_bitset<>::npos);
}

TEST(DynamicBitsetTest, IntegerConversionsDetectOverflow) {
  chaistl::dynamic_bitset<> value(128);
  value.set(3);
  EXPECT_EQ(value.to_ulong(), 8ul);
  EXPECT_EQ(value.to_ullong(), 8ull);

  value.set(std::numeric_limits<unsigned long>::digits);
  EXPECT_THROW((void)value.to_ulong(), std::overflow_error);

  chaistl::dynamic_bitset<> wide(128);
  wide.set(std::numeric_limits<unsigned long long>::digits);
  EXPECT_THROW((void)wide.to_ullong(), std::overflow_error);
}

TEST(DynamicBitsetTest, StreamInsertionAndExtractionUseTextRepresentation) {
  chaistl::dynamic_bitset<> value(8, 0b1010'0011);
  std::stringstream stream;

  stream << value;
  EXPECT_EQ(stream.str(), "10100011");

  chaistl::dynamic_bitset<> parsed;
  stream >> parsed;
  EXPECT_EQ(parsed, value);
}

TEST(DynamicBitsetTest, StreamExtractionGrowsAndStopsAtFirstNonBitCharacter) {
  std::stringstream stream("1012");
  chaistl::dynamic_bitset<> parsed;

  stream >> parsed;

  EXPECT_FALSE(stream.fail());
  EXPECT_EQ(parsed.size(), 3);
  EXPECT_EQ(parsed.to_string(), "101");
  EXPECT_EQ(static_cast<char>(stream.peek()), '2');
}

TEST(DynamicBitsetTest, HashesEqualValuesConsistently) {
  const chaistl::dynamic_bitset<> lhs(16, 0x1234);
  const chaistl::dynamic_bitset<> rhs(16, 0x1234);
  const chaistl::dynamic_bitset<> same_bits_different_size(17, 0x1234);
  const chaistl::dynamic_bitset<> other(16, 0x1235);

  EXPECT_EQ(chaistl::hash<chaistl::dynamic_bitset<>>{}(lhs), chaistl::hash<chaistl::dynamic_bitset<>>{}(rhs));
  EXPECT_EQ(std::hash<chaistl::dynamic_bitset<>>{}(lhs), std::hash<chaistl::dynamic_bitset<>>{}(rhs));
  EXPECT_NE(chaistl::hash<chaistl::dynamic_bitset<>>{}(lhs),
            chaistl::hash<chaistl::dynamic_bitset<>>{}(same_bits_different_size));
  EXPECT_NE(chaistl::hash<chaistl::dynamic_bitset<>>{}(lhs), chaistl::hash<chaistl::dynamic_bitset<>>{}(other));
}

}  // namespace
