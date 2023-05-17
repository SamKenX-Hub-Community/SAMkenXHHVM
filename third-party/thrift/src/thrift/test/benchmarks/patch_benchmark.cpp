/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <random>
#include <string>
#include <type_traits>

#include <folly/Benchmark.h>
#include <folly/init/Init.h>
#include <thrift/lib/cpp2/op/Get.h>
#include <thrift/lib/cpp2/op/Patch.h>
#include <thrift/test/gen-cpp2/StructPatchTest_types.h>

namespace apache::thrift::test::patch {

using ListPatch = folly::remove_cvref_t<
    decltype(*std::declval<MyStructFieldPatch>()->optListVal())>;
using ListDequePatch = folly::remove_cvref_t<
    decltype(*std::declval<MyStructFieldPatch>()->longList())>;
using SetPatch = folly::remove_cvref_t<
    decltype(*std::declval<MyStructFieldPatch>()->optSetVal())>;
using MapPatch = folly::remove_cvref_t<
    decltype(*std::declval<MyStructFieldPatch>()->optMapVal())>;
using ListStringPatch = folly::remove_cvref_t<
    decltype(*std::declval<StringsFieldPatch>()->strings())>;

// We apply/merge the patch N times to get a fairly complex patching.
// This is also used as the max index and key in list/set/map patch.
constexpr int N = 10;

std::mt19937 rng;
auto randInt() {
  return rng() % N;
}
auto randStr() {
  return std::to_string(randInt());
}
auto randLongStr() {
  return std::string(randInt() * 10, '0');
}

template <class PatchGenerator>
void benchmarkApply(PatchGenerator gen) {
  folly::BenchmarkSuspender susp;
  rng.seed(0);
  auto patch = gen();
  typename decltype(patch)::value_type value;
  for (auto i = 0; i < N; i++) {
    susp.dismiss();
    std::move(patch).apply(value);
    susp.rehire();
    patch = gen();
  }
}

template <class PatchGenerator>
void benchmarkMerge(PatchGenerator gen) {
  folly::BenchmarkSuspender susp;
  rng.seed(0);
  auto patch = gen(), next = gen();
  for (auto i = 0; i < N; i++) {
    susp.dismiss();
    patch.merge(std::move(next));
    susp.rehire();
    next = gen();
  }
}

ListPatch genListPatch() {
  ListPatch p;
  p.push_front(randInt());
  p.push_back(randInt());
  p.erase(randInt());
  p.patchAt(randInt()) += randInt();
  return p;
}

BENCHMARK(ApplyListPatch) {
  benchmarkApply(genListPatch);
}
BENCHMARK(MergeListPatch) {
  benchmarkMerge(genListPatch);
}

ListDequePatch genListDequePatch() {
  ListDequePatch p;
  p.push_front(randInt());
  p.push_back(randInt());
  p.erase(randInt());
  p.patchAt(randInt()) += randInt();
  return p;
}

BENCHMARK(ApplyListDequePatch) {
  benchmarkApply(genListDequePatch);
}
BENCHMARK(MergeListDequePatch) {
  benchmarkMerge(genListDequePatch);
}

SetPatch genSetPatch() {
  SetPatch p;
  p.insert(randStr());
  p.erase(randStr());
  return p;
}

BENCHMARK(ApplySetPatch) {
  benchmarkApply(genSetPatch);
}
BENCHMARK(MergeSetPatch) {
  benchmarkMerge(genSetPatch);
}

MapPatch genMapPatch() {
  MapPatch p;
  p.erase(randStr());
  p.patchByKey(randStr()) += randStr();
  p.ensureAndPatchByKey(randStr()) += randStr();
  return p;
}

BENCHMARK(ApplyMapPatch) {
  benchmarkApply(genMapPatch);
}
BENCHMARK(MergeMapPatch) {
  benchmarkMerge(genMapPatch);
}

void patchIfSetNonOptionalFields(MyStructPatch& result) {
  result.patchIfSet<ident::boolVal>() = !op::BoolPatch{};
  result.patchIfSet<ident::byteVal>() += 1;
  result.patchIfSet<ident::i16Val>() += 2;
  result.patchIfSet<ident::i32Val>() += 3;
  result.patchIfSet<ident::i64Val>() += 4;
  result.patchIfSet<ident::floatVal>() += 5;
  result.patchIfSet<ident::doubleVal>() += 6;
  result.patchIfSet<ident::stringVal>() = "(" + op::StringPatch{} + ")";
  result.patchIfSet<ident::binaryVal>() = "<" + op::BinaryPatch{} + ">";
  result.patchIfSet<ident::enumVal>() = MyEnum::MyValue9;
  result.patchIfSet<ident::structVal>().patchIfSet<ident::data1>().append("X");
  result.patchIfSet<ident::unionVal>().patchIfSet<ident::option1>().append("Y");
  result.patchIfSet<ident::longList>() = genListDequePatch();
}

void patchIfSetOptionalFields(MyStructPatch& result) {
  result.patchIfSet<ident::optBoolVal>() = !op::BoolPatch{};
  result.patchIfSet<ident::optByteVal>() += 1;
  result.patchIfSet<ident::optI16Val>() += 2;
  result.patchIfSet<ident::optI32Val>() += 3;
  result.patchIfSet<ident::optI64Val>() += 4;
  result.patchIfSet<ident::optFloatVal>() += 5;
  result.patchIfSet<ident::optDoubleVal>() += 6;
  result.patchIfSet<ident::optStringVal>() = "(" + op::StringPatch{} + ")";
  result.patchIfSet<ident::optBinaryVal>() = "<" + op::BinaryPatch{} + ">";
  result.patchIfSet<ident::optEnumVal>() = MyEnum::MyValue9;
  result.patchIfSet<ident::optStructVal>().patchIfSet<ident::data1>().append(
      "X");
  result.patchIfSet<ident::optListVal>() = genListPatch();
  result.patchIfSet<ident::optSetVal>() = genSetPatch();
  result.patchIfSet<ident::optMapVal>() = genMapPatch();
}

void ensureNonOptionalFields(MyStructPatch& result) {
  result.ensure<ident::boolVal>(true);
  result.ensure<ident::byteVal>(1);
  result.ensure<ident::i16Val>(2);
  result.ensure<ident::i32Val>(3);
  result.ensure<ident::i64Val>(4);
  result.ensure<ident::floatVal>(5);
  result.ensure<ident::doubleVal>(6);
  result.ensure<ident::stringVal>("7");
  result.ensure<ident::binaryVal>(folly::IOBuf::wrapBufferAsValue("8", 1));
  result.ensure<ident::enumVal>(MyEnum::MyValue9);
  result.ensure<ident::structVal>([] {
    MyData data;
    data.data1() = "10";
    return data;
  }());
  result.ensure<ident::unionVal>([] {
    MyUnion u;
    u.option1_ref() = "11";
    return u;
  }());
  result.ensure<ident::longList>({12});
}

void ensureOptionalFields(MyStructPatch& result) {
  result.ensure<ident::optBoolVal>(true);
  result.ensure<ident::optByteVal>(1);
  result.ensure<ident::optI16Val>(2);
  result.ensure<ident::optI32Val>(3);
  result.ensure<ident::optI64Val>(4);
  result.ensure<ident::optFloatVal>(5);
  result.ensure<ident::optDoubleVal>(6);
  result.ensure<ident::optStringVal>("7");
  result.ensure<ident::optBinaryVal>(folly::IOBuf::wrapBufferAsValue("8", 1));
  result.ensure<ident::optEnumVal>(MyEnum::MyValue9);
  result.ensure<ident::optStructVal>([] {
    MyData data;
    data.data1() = "10";
    return data;
  }());
  result.ensure<ident::optListVal>({11});
  result.ensure<ident::optSetVal>({"10", "20"});
  result.ensure<ident::optMapVal>({{"10", "1"}, {"20", "2"}});
}

MyStructPatch genComplexPatch() {
  MyStructPatch patch;
  patchIfSetNonOptionalFields(patch);
  patchIfSetOptionalFields(patch);
  ensureNonOptionalFields(patch);
  ensureOptionalFields(patch);
  patchIfSetNonOptionalFields(patch);
  patchIfSetOptionalFields(patch);
  return patch;
}

BENCHMARK(ApplyComplexPatch) {
  benchmarkApply(genComplexPatch);
}

BENCHMARK(MergeComplexPatch) {
  benchmarkMerge(genComplexPatch);
}

ListStringPatch genListLongStringPatch() {
  ListStringPatch p;
  p.push_front(randLongStr());
  p.push_back(randLongStr());
  p.erase(randLongStr());
  p.patchAt(randInt()) += randLongStr();
  return p;
}

BENCHMARK(ApplyListLongStringPatch) {
  benchmarkApply(genListLongStringPatch);
}

BENCHMARK(MergeListLongStringPatch) {
  benchmarkApply(genListLongStringPatch);
}

} // namespace apache::thrift::test::patch

int main(int argc, char** argv) {
  folly::init(&argc, &argv);
  folly::runBenchmarks();
}
