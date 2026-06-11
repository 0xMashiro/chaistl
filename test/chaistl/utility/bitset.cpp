// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/bitset.hpp>

#include <cstddef>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

static_assert(chaistl::bitset<0>{}.size() == 0);
static_assert(chaistl::bitset<0>{}.all());
static_assert(chaistl::bitset<0>{}.none());
static_assert(!chaistl::bitset<0>{}.any());

static_assert(chaistl::bitset<8>{0b1010'0101}.count() == 4);
static_assert(chaistl::bitset<8>{0b1010'0101}.test(0));
static_assert(!chaistl::bitset<8>{0b1010'0101}.test(1));
static_assert((chaistl::bitset<8>{0b0000'1111} << 4).to_ullong() == 0b1111'0000);
static_assert((chaistl::bitset<8>{0b1111'0000} >> 4).to_ullong() == 0b0000'1111);
static_assert((chaistl::bitset<8>{0b1100} & chaistl::bitset<8>{0b1010}).to_ullong() == 0b1000);
static_assert((chaistl::bitset<8>{0b1100} | chaistl::bitset<8>{0b1010}).to_ullong() == 0b1110);
static_assert((chaistl::bitset<8>{0b1100} ^ chaistl::bitset<8>{0b1010}).to_ullong() == 0b0110);

static_assert(std::is_same_v<decltype(chaistl::bitset<4>{}[0]), chaistl::bitset<4>::reference>);
static_assert(std::is_same_v<decltype(static_cast<const chaistl::bitset<4>&>(chaistl::bitset<4>{})[0]), bool>);

TEST(BitsetTest, ConstructsFromIntegerAndTrimsUnusedBits) {
  const chaistl::bitset<4> value{0b1111'1111};

  EXPECT_EQ(value.size(), 4);
  EXPECT_EQ(value.count(), 4);
  EXPECT_TRUE(value.all());
  EXPECT_EQ(value.to_ullong(), 0b1111ull);
  EXPECT_EQ(value.to_string(), "1111");
}

TEST(BitsetTest, StringConstructorUsesRightmostCharacterAsBitZero) {
  const chaistl::bitset<8> value{std::string("00101001")};

  EXPECT_TRUE(value.test(0));
  EXPECT_FALSE(value.test(1));
  EXPECT_FALSE(value.test(2));
  EXPECT_TRUE(value.test(3));
  EXPECT_EQ(value.to_ullong(), 0b0010'1001ull);
  EXPECT_EQ(value.to_string(), "00101001");
}

TEST(BitsetTest, StringConstructorHonorsPositionCountAndCustomDigits) {
  const chaistl::bitset<4> value{std::string("xxABBAyy"), 2, 4, 'A', 'B'};

  EXPECT_EQ(value.to_string('A', 'B'), "ABBA");
  EXPECT_EQ(value.to_ullong(), 0b0110ull);
}

TEST(BitsetTest, StringConstructorValidatesInput) {
  EXPECT_THROW((chaistl::bitset<4>{std::string("1010"), 5}), std::out_of_range);
  EXPECT_THROW((chaistl::bitset<4>{std::string("10x0")}), std::invalid_argument);
}

TEST(BitsetTest, ReferenceProxyMutatesBits) {
  chaistl::bitset<8> value;

  value[1] = true;
  value[4] = value[1];
  value[1].flip();

  EXPECT_FALSE(value[1]);
  EXPECT_TRUE(value[4]);
  EXPECT_TRUE(~value[1]);
  EXPECT_EQ(value.to_ullong(), 0b0001'0000ull);
}

TEST(BitsetTest, ModifiersAndQueriesMatchBitsetSemantics) {
  chaistl::bitset<10> value;

  EXPECT_TRUE(value.none());
  EXPECT_FALSE(value.any());
  EXPECT_FALSE(value.all());

  value.set();
  EXPECT_TRUE(value.all());
  EXPECT_EQ(value.count(), 10);

  value.reset(3).reset(9);
  EXPECT_FALSE(value.all());
  EXPECT_EQ(value.count(), 8);

  value.flip(3).flip(0);
  EXPECT_TRUE(value.test(3));
  EXPECT_FALSE(value.test(0));

  value.reset();
  EXPECT_TRUE(value.none());
}

TEST(BitsetTest, ThrowsForCheckedOutOfRangeAccess) {
  chaistl::bitset<4> value;

  EXPECT_THROW((void)value.test(4), std::out_of_range);
  EXPECT_THROW(value.set(4), std::out_of_range);
  EXPECT_THROW(value.reset(4), std::out_of_range);
  EXPECT_THROW(value.flip(4), std::out_of_range);
}

TEST(BitsetTest, BitwiseOperatorsDoNotLeakUnusedTailBits) {
  chaistl::bitset<70> lhs;
  chaistl::bitset<70> rhs;

  lhs.set(69).set(1);
  rhs.set(69).set(2);

  EXPECT_EQ((lhs & rhs).count(), 1);
  EXPECT_TRUE((lhs & rhs).test(69));
  EXPECT_EQ((lhs | rhs).count(), 3);
  EXPECT_EQ((lhs ^ rhs).count(), 2);
  EXPECT_EQ((~chaistl::bitset<70>{}).count(), 70);
}

TEST(BitsetTest, ShiftOperatorsCrossWordBoundaries) {
  chaistl::bitset<130> value;
  value.set(0).set(63).set(64);

  const auto shifted_left = value << 65;
  EXPECT_TRUE(shifted_left.test(65));
  EXPECT_TRUE(shifted_left.test(128));
  EXPECT_TRUE(shifted_left.test(129));
  EXPECT_EQ(shifted_left.count(), 3);

  const auto shifted_right = shifted_left >> 65;
  EXPECT_EQ(shifted_right, value);

  value <<= 130;
  EXPECT_TRUE(value.none());
}

TEST(BitsetTest, IntegerConversionsDetectOverflow) {
  chaistl::bitset<128> value;
  value.set(3);
  EXPECT_EQ(value.to_ulong(), 8ul);
  EXPECT_EQ(value.to_ullong(), 8ull);

  value.set(std::numeric_limits<unsigned long>::digits);
  EXPECT_THROW((void)value.to_ulong(), std::overflow_error);

  chaistl::bitset<128> wide;
  wide.set(std::numeric_limits<unsigned long long>::digits);
  EXPECT_THROW((void)wide.to_ullong(), std::overflow_error);
}

TEST(BitsetTest, StreamInsertionAndExtractionUseTextRepresentation) {
  chaistl::bitset<8> value{0b1010'0011};
  std::stringstream stream;

  stream << value;
  EXPECT_EQ(stream.str(), "10100011");

  chaistl::bitset<8> parsed;
  stream >> parsed;
  EXPECT_EQ(parsed, value);
}

TEST(BitsetTest, StreamExtractionStopsAtFirstNonBitCharacter) {
  std::stringstream stream("1012");
  chaistl::bitset<8> parsed;

  stream >> parsed;

  EXPECT_FALSE(stream.fail());
  EXPECT_EQ(parsed.to_ullong(), 0b101ull);
  EXPECT_EQ(static_cast<char>(stream.peek()), '2');
}

TEST(BitsetTest, ZeroSizeStreamExtractionDoesNotConsumeOrFail) {
  std::stringstream stream("101");
  chaistl::bitset<0> parsed;

  stream >> parsed;

  EXPECT_FALSE(stream.fail());
  EXPECT_EQ(stream.tellg(), std::streampos(0));
  EXPECT_EQ(static_cast<char>(stream.peek()), '1');
}

TEST(BitsetTest, HashesEqualValuesConsistently) {
  const chaistl::bitset<16> lhs{0x1234};
  const chaistl::bitset<16> rhs{0x1234};
  const chaistl::bitset<16> other{0x1235};

  EXPECT_EQ(chaistl::hash<chaistl::bitset<16>>{}(lhs), chaistl::hash<chaistl::bitset<16>>{}(rhs));
  EXPECT_EQ(std::hash<chaistl::bitset<16>>{}(lhs), std::hash<chaistl::bitset<16>>{}(rhs));
  EXPECT_NE(chaistl::hash<chaistl::bitset<16>>{}(lhs), chaistl::hash<chaistl::bitset<16>>{}(other));
}

}  // namespace
