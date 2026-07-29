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
 * Unit tests for the C++ generator option `pure`.
 *
 * Pure types are emitted in a separate *_types_pure.h that does not include
 * thrift runtime headers (TBase / protocol / transport). Classic types remain
 * in *_types.h.
 *
 * Uses existing thrift IDL under test/:
 *   OptionalRequiredTest.thrift  — optional fields
 *   Recursive.thrift             — unions and recursive types
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

// Pure header: no thrift runtime includes required for pure:: types.
#include "gen-cpp-pure/OptionalRequiredTest_types_pure.h"
#include "gen-cpp-pure/Recursive_types_pure.h"

// Classic header + convert helpers (to_pure / from_pure).
#include "gen-cpp-pure/OptionalRequiredTest_types_pure_convert.h"
#include "gen-cpp-pure/Recursive_types_pure_convert.h"

#define BOOST_TEST_MODULE PureTypesTest
#include <boost/test/unit_test.hpp>

using thrift::test::pure::Binaries;
using thrift::test::pure::Complex;
using thrift::test::pure::ManyOpt;
using thrift::test::pure::OptionalDefault;
using thrift::test::pure::Simple;
using thrift::test::pure::Tricky2;
using thrift::test::pure::Tricky3;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

template <typename T>
constexpr bool is_std_optional_v = false;

template <typename U>
constexpr bool is_std_optional_v<std::optional<U>> = true;

template <typename T>
constexpr bool is_std_variant_v = false;

template <typename... Ts>
constexpr bool is_std_variant_v<std::variant<Ts...>> = true;

// ---------------------------------------------------------------------------
// Pure header independence
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pure_header_has_no_tbase_dependency) {
  // Compiling this translation unit against *_types_pure.h alone is enough
  // for pure types; classic TBase types are a separate include used only for
  // coexistence checks below.
  Simple s;
  s.im_optional = 1;
  BOOST_CHECK(s.im_optional.has_value());
}

// ---------------------------------------------------------------------------
// Optional fields from OptionalRequiredTest.thrift
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pure_optional_fields_use_std_optional) {
  static_assert(is_std_optional_v<decltype(Simple::im_optional)>);
  static_assert(!is_std_optional_v<decltype(Simple::im_default)>);
  static_assert(!is_std_optional_v<decltype(Simple::im_required)>);
  static_assert(is_std_optional_v<decltype(Tricky2::im_optional)>);
  static_assert(!is_std_optional_v<decltype(Tricky3::im_required)>);
  static_assert(is_std_optional_v<decltype(Complex::cp_optional)>);
  static_assert(is_std_optional_v<decltype(Complex::opt_simp)>);
  static_assert(!is_std_optional_v<decltype(Complex::req_simp)>);
  static_assert(is_std_optional_v<decltype(Binaries::opt_bin)>);

  Simple s;
  BOOST_CHECK(!s.im_optional.has_value());
  BOOST_CHECK_EQUAL(s.im_default, 0);
  BOOST_CHECK_EQUAL(s.im_required, 0);

  s.im_optional = 42;
  BOOST_REQUIRE(s.im_optional.has_value());
  BOOST_CHECK_EQUAL(*s.im_optional, 42);

  s.im_optional.reset();
  BOOST_CHECK(!s.im_optional.has_value());
}

BOOST_AUTO_TEST_CASE(pure_optional_defaults_are_initialized) {
  OptionalDefault o;
  BOOST_REQUIRE(o.opt_int.has_value());
  BOOST_CHECK_EQUAL(*o.opt_int, 1234);
  BOOST_REQUIRE(o.opt_str.has_value());
  BOOST_CHECK_EQUAL(*o.opt_str, "default");
}

BOOST_AUTO_TEST_CASE(pure_nested_struct_fields) {
  Complex c;
  c.cp_required = 7;
  c.req_simp.im_required = 9;
  c.req_simp.im_optional = 11;
  c.opt_simp = Simple{};
  c.opt_simp->im_default = 3;
  c.opt_simp->im_optional = 5;

  BOOST_CHECK_EQUAL(c.cp_required, 7);
  BOOST_CHECK_EQUAL(c.req_simp.im_required, 9);
  BOOST_REQUIRE(c.req_simp.im_optional.has_value());
  BOOST_CHECK_EQUAL(*c.req_simp.im_optional, 11);
  BOOST_REQUIRE(c.opt_simp.has_value());
  BOOST_CHECK_EQUAL(c.opt_simp->im_default, 3);
  BOOST_REQUIRE(c.opt_simp->im_optional.has_value());
  BOOST_CHECK_EQUAL(*c.opt_simp->im_optional, 5);
}

BOOST_AUTO_TEST_CASE(pure_many_optional_fields) {
  ManyOpt m;
  BOOST_CHECK(!m.opt1.has_value());
  BOOST_CHECK(!m.opt2.has_value());
  BOOST_CHECK_EQUAL(m.def4, 0);

  m.opt1 = 1;
  m.opt6 = 6;
  BOOST_CHECK_EQUAL(*m.opt1, 1);
  BOOST_CHECK_EQUAL(*m.opt6, 6);
}

// Classic types remain available in parallel (separate header).
BOOST_AUTO_TEST_CASE(pure_and_classic_types_coexist) {
  thrift::test::Simple classic;
  thrift::test::pure::Simple pure_s;

  classic.im_required = 1;
  classic.__isset.im_optional = true;
  classic.im_optional = 2;

  pure_s.im_required = 1;
  pure_s.im_optional = 2;

  BOOST_CHECK_EQUAL(classic.im_required, pure_s.im_required);
  BOOST_REQUIRE(pure_s.im_optional.has_value());
  BOOST_CHECK_EQUAL(classic.im_optional, *pure_s.im_optional);
  BOOST_CHECK(classic.__isset.im_optional);
}

// ---------------------------------------------------------------------------
// to_pure / from_pure conversions
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pure_convert_optional_roundtrip) {
  using thrift::test::to_pure;
  using thrift::test::from_pure;

  thrift::test::Simple classic;
  classic.im_default = 1;
  classic.im_required = 2;
  classic.__isset.im_optional = true;
  classic.im_optional = 3;

  thrift::test::pure::Simple p = to_pure(classic);
  BOOST_CHECK_EQUAL(p.im_default, 1);
  BOOST_CHECK_EQUAL(p.im_required, 2);
  BOOST_REQUIRE(p.im_optional.has_value());
  BOOST_CHECK_EQUAL(*p.im_optional, 3);

  thrift::test::Simple back = from_pure(p);
  BOOST_CHECK_EQUAL(back.im_default, 1);
  BOOST_CHECK_EQUAL(back.im_required, 2);
  BOOST_CHECK(back.__isset.im_optional);
  BOOST_CHECK_EQUAL(back.im_optional, 3);

  // Unset optional stays unset through conversion.
  classic.__isset.im_optional = false;
  thrift::test::pure::Simple p2 = to_pure(classic);
  BOOST_CHECK(!p2.im_optional.has_value());
  thrift::test::Simple back2 = from_pure(p2);
  BOOST_CHECK(!back2.__isset.im_optional);
}

BOOST_AUTO_TEST_CASE(pure_convert_nested_struct_roundtrip) {
  using thrift::test::to_pure;
  using thrift::test::from_pure;

  thrift::test::Complex classic;
  classic.cp_default = 1;
  classic.cp_required = 2;
  classic.__isset.cp_optional = true;
  classic.cp_optional = 3;
  classic.req_simp.im_required = 4;
  classic.req_simp.__isset.im_optional = true;
  classic.req_simp.im_optional = 5;
  classic.__isset.opt_simp = true;
  classic.opt_simp.im_default = 6;
  classic.opt_simp.im_required = 7;

  thrift::test::Simple mapped;
  mapped.im_required = 8;
  classic.the_map[10] = mapped;

  thrift::test::pure::Complex p = to_pure(classic);
  BOOST_CHECK_EQUAL(p.cp_required, 2);
  BOOST_REQUIRE(p.cp_optional.has_value());
  BOOST_CHECK_EQUAL(*p.cp_optional, 3);
  BOOST_CHECK_EQUAL(p.req_simp.im_required, 4);
  BOOST_REQUIRE(p.req_simp.im_optional.has_value());
  BOOST_CHECK_EQUAL(*p.req_simp.im_optional, 5);
  BOOST_REQUIRE(p.opt_simp.has_value());
  BOOST_CHECK_EQUAL(p.opt_simp->im_default, 6);
  BOOST_REQUIRE_EQUAL(p.the_map.size(), 1u);
  BOOST_CHECK_EQUAL(p.the_map.at(10).im_required, 8);

  thrift::test::Complex back = from_pure(p);
  BOOST_CHECK_EQUAL(back.cp_required, 2);
  BOOST_CHECK(back.__isset.cp_optional);
  BOOST_CHECK_EQUAL(back.cp_optional, 3);
  BOOST_CHECK(back.__isset.opt_simp);
  BOOST_CHECK_EQUAL(back.opt_simp.im_default, 6);
  BOOST_CHECK_EQUAL(back.the_map[10].im_required, 8);
}

BOOST_AUTO_TEST_CASE(pure_convert_union_and_recursive_roundtrip) {
  // Recursive.thrift has no cpp namespace — converters are in the global scope.

  RecList classic_list;
  classic_list.item = 1;
  classic_list.nextitem = std::make_shared<RecList>();
  classic_list.nextitem->item = 2;

  pure::RecList pure_list = to_pure(classic_list);
  BOOST_CHECK_EQUAL(pure_list.item, 1);
  BOOST_REQUIRE(pure_list.nextitem);
  BOOST_CHECK_EQUAL(pure_list.nextitem->item, 2);

  RecList back_list = from_pure(pure_list);
  BOOST_CHECK_EQUAL(back_list.item, 1);
  BOOST_REQUIRE(back_list.nextitem);
  BOOST_CHECK_EQUAL(back_list.nextitem->item, 2);

  CoUnion classic_u;
  classic_u.__set_other(std::make_shared<CoUnion2>());
  classic_u.other->__set_other(CoUnion());

  pure::CoUnion pure_u = to_pure(classic_u);
  BOOST_CHECK(pure_u.is_set_other());
  BOOST_REQUIRE(pure_u.get_other());
  BOOST_CHECK(pure_u.get_other()->is_set_other());

  CoUnion back_u = from_pure(pure_u);
  BOOST_CHECK(back_u.__isset.other);
  BOOST_REQUIRE(back_u.other);
  BOOST_CHECK(back_u.other->__isset.other);
}

// ---------------------------------------------------------------------------
// Unions and recursive types from Recursive.thrift
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(pure_union_uses_std_variant) {
  using CoUnion = pure::CoUnion;
  using CoUnion2 = pure::CoUnion2;

  static_assert(is_std_variant_v<CoUnion::value_type>);
  static_assert(is_std_variant_v<CoUnion2::value_type>);
  static_assert(std::variant_size_v<CoUnion::value_type> == 2);
  static_assert(std::variant_size_v<CoUnion2::value_type> == 2);

  CoUnion u;
  BOOST_CHECK(u.get_field_id() == CoUnion::field_id::__EMPTY__);
  BOOST_CHECK(!u.is_set_other());

  auto inner = std::make_shared<CoUnion2>();
  u.set_other(inner);
  BOOST_CHECK(u.is_set_other());
  BOOST_REQUIRE(u.get_other());
  BOOST_CHECK(u.get_field_id() == CoUnion::field_id::other);

  CoUnion nested;
  nested.set_other(std::make_shared<CoUnion2>());
  inner->set_other(nested);
  BOOST_REQUIRE(u.get_other()->is_set_other());
  BOOST_CHECK(u.get_other()->get_other().is_set_other());

  u.value = std::monostate{};
  BOOST_CHECK(std::holds_alternative<std::monostate>(u.value));
  BOOST_CHECK(u.get_field_id() == CoUnion::field_id::__EMPTY__);
}

BOOST_AUTO_TEST_CASE(pure_recursive_struct_value_types) {
  pure::RecTree tree;
  tree.item = 1;
  pure::RecTree child;
  child.item = 2;
  tree.children.push_back(child);

  BOOST_CHECK_EQUAL(tree.item, 1);
  BOOST_REQUIRE_EQUAL(tree.children.size(), 1u);
  BOOST_CHECK_EQUAL(tree.children[0].item, 2);

  pure::RecList node;
  node.item = 10;
  node.nextitem = std::make_shared<pure::RecList>();
  node.nextitem->item = 20;
  BOOST_CHECK_EQUAL(node.item, 10);
  BOOST_REQUIRE(node.nextitem);
  BOOST_CHECK_EQUAL(node.nextitem->item, 20);
}

BOOST_AUTO_TEST_CASE(pure_namespace_exists_alongside_classic_recursive) {
  RecTree classic_tree;
  pure::RecTree pure_tree;
  classic_tree.item = 1;
  pure_tree.item = 1;
  BOOST_CHECK_EQUAL(classic_tree.item, pure_tree.item);
}
