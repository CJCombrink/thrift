/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * Wire-format equivalence tests for the cpp:pure generator option.
 *
 * Pure types themselves have no read/write; they go on the wire via classic
 * types after from_pure(). These tests assert that classic → pure → classic
 * produces the same serialized bytes as the original classic value, for the
 * same protocols exercised by AllProtocolTests / JSONProtoTest /
 * OptionalRequiredTest.
 */

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/protocol/TJSONProtocol.h>
#include <thrift/transport/TBufferTransports.h>

#include "gen-cpp-pure/DebugProtoTest_types_pure_convert.h"
#include "gen-cpp-pure/OptionalRequiredTest_types_pure_convert.h"
#include "gen-cpp-pure/Recursive_types_pure_convert.h"

#define BOOST_TEST_MODULE PureProtocolTest
#include <boost/test/unit_test.hpp>

using std::shared_ptr;
using std::string;
using apache::thrift::protocol::TBinaryProtocol;
using apache::thrift::protocol::TCompactProtocol;
using apache::thrift::protocol::TJSONProtocol;
using apache::thrift::protocol::TLEBinaryProtocol;
using apache::thrift::protocol::TProtocol;
using apache::thrift::transport::TMemoryBuffer;
using apache::thrift::transport::TTransport;

// ---------------------------------------------------------------------------
// Serialization helpers (same pattern as OptionalRequiredTest / TMemoryBufferTest)
// ---------------------------------------------------------------------------

template <typename Protocol>
string serialize_struct(const apache::thrift::TBase& value) {
  shared_ptr<TMemoryBuffer> buffer(new TMemoryBuffer());
  shared_ptr<TProtocol> protocol(new Protocol(buffer));
  value.write(protocol.get());
  return buffer->getBufferAsString();
}

template <typename Protocol, typename Struct>
void deserialize_struct(const string& data, Struct& out) {
  shared_ptr<TMemoryBuffer> buffer(new TMemoryBuffer());
  buffer->resetBuffer(reinterpret_cast<uint8_t*>(const_cast<char*>(data.data())),
                      static_cast<uint32_t>(data.size()));
  shared_ptr<TProtocol> protocol(new Protocol(buffer));
  out.read(protocol.get());
}

/**
 * Write classic A, convert A → pure → classic B, write B; bytes must match.
 * Also: read A from wire into C, convert C → pure → classic D, write D; match.
 *
 * Convert callables are passed explicitly so overloads from multiple thrift
 * namespaces (and the global Recursive.thrift converters) do not collide.
 */
template <typename Protocol, typename Classic, typename ToPure, typename FromPure>
void assert_pure_preserves_wire(const Classic& original,
                                ToPure to_p,
                                FromPure from_p,
                                const string& label) {
  const string direct = serialize_struct<Protocol>(original);

  Classic via_pure = from_p(to_p(original));
  const string after_convert = serialize_struct<Protocol>(via_pure);

  BOOST_CHECK_MESSAGE(direct == after_convert,
                      "to_pure/from_pure changed wire bytes (" << label << "); "
                      "direct size=" << direct.size()
                      << " converted size=" << after_convert.size());

  Classic decoded;
  deserialize_struct<Protocol>(direct, decoded);
  Classic decoded_via_pure = from_p(to_p(decoded));
  const string after_decode_convert = serialize_struct<Protocol>(decoded_via_pure);

  BOOST_CHECK_MESSAGE(direct == after_decode_convert,
                      "decode+to_pure/from_pure changed wire bytes (" << label << ")");

  // Re-serialize the pure-roundtripped decode; must match the original wire.
  // (Do not use operator== here: thrift compares shared_ptr fields by pointer
  // identity, so two independent deserializations of the same bytes fail ==.)
  (void)decoded;
}

template <typename Classic, typename ToPure, typename FromPure>
void assert_pure_preserves_wire_all_protocols(const Classic& original,
                                              ToPure to_p,
                                              FromPure from_p,
                                              const string& what) {
  assert_pure_preserves_wire<TBinaryProtocol>(original, to_p, from_p, what + "/TBinaryProtocol");
  assert_pure_preserves_wire<TLEBinaryProtocol>(original, to_p, from_p, what + "/TLEBinaryProtocol");
  assert_pure_preserves_wire<TCompactProtocol>(original, to_p, from_p, what + "/TCompactProtocol");
  assert_pure_preserves_wire<TJSONProtocol>(original, to_p, from_p, what + "/TJSONProtocol");
}

// write_to_read style from OptionalRequiredTest: compare wire after pure, not
// operator== (shared_ptr fields compare by identity).
template <typename Classic, typename ToPure, typename FromPure>
void assert_write_to_read_via_pure(const Classic& original, ToPure to_p, FromPure from_p) {
  const string direct = serialize_struct<TBinaryProtocol>(original);
  Classic converted = from_p(to_p(original));
  const string via_pure = serialize_struct<TBinaryProtocol>(converted);
  BOOST_CHECK(direct == via_pure);

  Classic direct_read;
  deserialize_struct<TBinaryProtocol>(direct, direct_read);
  Classic convert_read;
  deserialize_struct<TBinaryProtocol>(via_pure, convert_read);
  BOOST_CHECK(serialize_struct<TBinaryProtocol>(direct_read)
              == serialize_struct<TBinaryProtocol>(from_p(to_p(direct_read))));
  BOOST_CHECK(serialize_struct<TBinaryProtocol>(convert_read)
              == serialize_struct<TBinaryProtocol>(from_p(to_p(convert_read))));
}

// Bind thrift::test::{to_pure,from_pure}
struct OptConvert {
  template <typename T>
  auto operator()(const T& v) const -> decltype(thrift::test::to_pure(v)) {
    return thrift::test::to_pure(v);
  }
  template <typename T>
  auto from(const T& v) const -> decltype(thrift::test::from_pure(v)) {
    return thrift::test::from_pure(v);
  }
};

// thrift::test::debug::{to_pure,from_pure}
struct DebugConvert {
  template <typename T>
  auto operator()(const T& v) const -> decltype(thrift::test::debug::to_pure(v)) {
    return thrift::test::debug::to_pure(v);
  }
  template <typename T>
  auto from(const T& v) const -> decltype(thrift::test::debug::from_pure(v)) {
    return thrift::test::debug::from_pure(v);
  }
};

// Global converters from Recursive.thrift (no cpp namespace)
struct RecursiveConvert {
  template <typename T>
  auto operator()(const T& v) const -> decltype(::to_pure(v)) {
    return ::to_pure(v);
  }
  template <typename T>
  auto from(const T& v) const -> decltype(::from_pure(v)) {
    return ::from_pure(v);
  }
};

template <typename Convert, typename Classic>
void check_wire(const Classic& value, Convert conv, const string& label) {
  auto to_p = [&](const Classic& c) { return conv(c); };
  using Pure = decltype(conv(value));
  auto from_p = [&](const Pure& p) { return conv.from(p); };
  assert_pure_preserves_wire_all_protocols(value, to_p, from_p, label);
  assert_write_to_read_via_pure(value, to_p, from_p);
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

namespace {

thrift::test::Simple make_simple(bool with_optional) {
  thrift::test::Simple s;
  s.im_default = 11;
  s.im_required = 22;
  if (with_optional) {
    s.__set_im_optional(33);
  }
  return s;
}

thrift::test::Complex make_complex() {
  thrift::test::Complex c;
  c.cp_default = 1;
  c.cp_required = 2;
  c.__set_cp_optional(3);
  c.req_simp = make_simple(true);
  thrift::test::Simple opt = make_simple(false);
  opt.im_default = 99;
  c.__set_opt_simp(opt);
  thrift::test::Simple mapped = make_simple(true);
  mapped.im_required = 8;
  c.the_map[10] = mapped;
  c.the_map[20] = make_simple(false);
  return c;
}

thrift::test::OldSchool make_old_school() {
  thrift::test::OldSchool o;
  o.im_int = 7;
  o.im_str = "hello";
  std::map<int32_t, std::string> m;
  m[1] = "a";
  m[2] = "b";
  o.im_big.push_back(m);
  return o;
}

thrift::test::Binaries make_binaries() {
  thrift::test::Binaries b;
  b.bin = std::string("\x00\x01\xff", 3);
  b.req_bin = std::string("req");
  b.__set_opt_bin(std::string("opt"));
  return b;
}

// Same seed values as DebugProtoTest / JSONProtoTest
thrift::test::debug::OneOfEach make_one_of_each() {
  thrift::test::debug::OneOfEach ooe;
  ooe.im_true = true;
  ooe.im_false = false;
  ooe.a_bite = 0x7f;
  ooe.integer16 = 27000;
  ooe.integer32 = 1 << 24;
  ooe.integer64 = (uint64_t)6000 * 1000 * 1000;
  ooe.double_precision = M_PI;
  ooe.some_characters = "JSON THIS! \"\1";
  ooe.zomg_unicode = "\xd7\n\a\t";
  ooe.base64 = "\1\2\3\255";
  ooe.rfc4122_uuid = apache::thrift::TUuid{"00000000-0000-0000-0000-000000000000"};
  return ooe;
}

thrift::test::debug::Nesting make_nesting() {
  thrift::test::debug::Nesting n;
  n.my_ooe = make_one_of_each();
  n.my_ooe.integer16 = 16;
  n.my_ooe.integer32 = 32;
  n.my_ooe.integer64 = 64;
  n.my_ooe.double_precision = (std::sqrt(5.0) + 1) / 2;
  n.my_ooe.some_characters = ":R (me going \"rrrr\")";
  n.my_ooe.rfc4122_uuid = apache::thrift::TUuid{"5e2ab188-1726-4e75-a04f-1ed9a6a89c4c"};
  n.my_bonk.type = 31337;
  n.my_bonk.message = "I am a bonk... xor!";
  return n;
}

thrift::test::debug::HolyMoley make_holy_moley() {
  thrift::test::debug::HolyMoley hm;
  thrift::test::debug::OneOfEach ooe = make_one_of_each();
  hm.big.push_back(ooe);
  ooe.integer16 = 1;
  hm.big.push_back(ooe);

  std::vector<std::string> stage1;
  stage1.push_back("and a one");
  stage1.push_back("and a two");
  hm.contain.insert(stage1);
  stage1.clear();
  stage1.push_back("then a one, two");
  stage1.push_back("three!");
  stage1.push_back("FOUR!!");
  hm.contain.insert(stage1);

  std::vector<thrift::test::debug::Bonk> stage2;
  thrift::test::debug::Bonk b;
  b.type = 1;
  b.message = "Wait.";
  stage2.push_back(b);
  b.type = 2;
  b.message = "What?";
  stage2.push_back(b);
  hm.bonks["zero"] = stage2;
  return hm;
}

RecList make_rec_list() {
  RecList head;
  head.item = 1;
  head.nextitem = std::make_shared<RecList>();
  head.nextitem->item = 2;
  head.nextitem->nextitem = std::make_shared<RecList>();
  head.nextitem->nextitem->item = 3;
  return head;
}

RecTree make_rec_tree() {
  RecTree root;
  root.item = 1;
  RecTree child;
  child.item = 2;
  RecTree leaf;
  leaf.item = 3;
  child.children.push_back(leaf);
  root.children.push_back(child);
  return root;
}

CoUnion make_co_union() {
  CoUnion u;
  auto inner = std::make_shared<CoUnion2>();
  CoUnion nested;
  nested.__set_other(std::make_shared<CoUnion2>());
  inner->__set_other(nested);
  u.__set_other(inner);
  return u;
}

} // namespace

// ---------------------------------------------------------------------------
// OptionalRequiredTest
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(wire_optional_simple_with_optional) {
  check_wire(make_simple(true), OptConvert{}, "Simple+opt");
}

BOOST_AUTO_TEST_CASE(wire_optional_simple_without_optional) {
  check_wire(make_simple(false), OptConvert{}, "Simple-opt");
}

BOOST_AUTO_TEST_CASE(wire_optional_complex) {
  check_wire(make_complex(), OptConvert{}, "Complex");
}

BOOST_AUTO_TEST_CASE(wire_optional_old_school) {
  check_wire(make_old_school(), OptConvert{}, "OldSchool");
}

BOOST_AUTO_TEST_CASE(wire_optional_binaries) {
  check_wire(make_binaries(), OptConvert{}, "Binaries");
}

BOOST_AUTO_TEST_CASE(wire_optional_defaults) {
  thrift::test::OptionalDefault o;
  check_wire(o, OptConvert{}, "OptionalDefault");

  // Explicitly cleared isset: pure convert must not re-introduce defaults on the wire.
  // (Decoding an empty struct into a fresh OptionalDefault is different — the classic
  // default ctor re-marks those optionals as set.)
  thrift::test::OptionalDefault cleared;
  cleared.__isset.opt_int = false;
  cleared.__isset.opt_str = false;
  auto to_p = [](const thrift::test::OptionalDefault& c) {
    return thrift::test::to_pure(c);
  };
  auto from_p = [](const thrift::test::pure::OptionalDefault& p) {
    return thrift::test::from_pure(p);
  };
  // Only the convert path (not decode-then-convert).
  BOOST_CHECK(serialize_struct<TBinaryProtocol>(cleared)
              == serialize_struct<TBinaryProtocol>(from_p(to_p(cleared))));
  BOOST_CHECK(serialize_struct<TCompactProtocol>(cleared)
              == serialize_struct<TCompactProtocol>(from_p(to_p(cleared))));
  BOOST_CHECK(serialize_struct<TJSONProtocol>(cleared)
              == serialize_struct<TJSONProtocol>(from_p(to_p(cleared))));
}

// ---------------------------------------------------------------------------
// DebugProtoTest
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(wire_debug_one_of_each) {
  check_wire(make_one_of_each(), DebugConvert{}, "OneOfEach");
}

BOOST_AUTO_TEST_CASE(wire_debug_nesting) {
  check_wire(make_nesting(), DebugConvert{}, "Nesting");
}

BOOST_AUTO_TEST_CASE(wire_debug_holy_moley) {
  check_wire(make_holy_moley(), DebugConvert{}, "HolyMoley");
}

BOOST_AUTO_TEST_CASE(wire_debug_backwards_field_order) {
  thrift::test::debug::Backwards b;
  b.first_tag2 = 42;
  b.second_tag1 = 17;
  check_wire(b, DebugConvert{}, "Backwards");
}

BOOST_AUTO_TEST_CASE(wire_debug_empty_and_wrapper) {
  thrift::test::debug::Empty e;
  check_wire(e, DebugConvert{}, "Empty");

  thrift::test::debug::Wrapper w;
  check_wire(w, DebugConvert{}, "Wrapper");
}

// ---------------------------------------------------------------------------
// Recursive.thrift
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(wire_recursive_list) {
  check_wire(make_rec_list(), RecursiveConvert{}, "RecList");
}

BOOST_AUTO_TEST_CASE(wire_recursive_tree) {
  check_wire(make_rec_tree(), RecursiveConvert{}, "RecTree");
}

BOOST_AUTO_TEST_CASE(wire_recursive_union) {
  check_wire(make_co_union(), RecursiveConvert{}, "CoUnion");
}

BOOST_AUTO_TEST_CASE(wire_empty_union_and_defaults) {
  CoUnion empty;
  check_wire(empty, RecursiveConvert{}, "CoUnion/empty");

  RecList empty_list;
  empty_list.item = 0;
  check_wire(empty_list, RecursiveConvert{}, "RecList/empty-next");
}
