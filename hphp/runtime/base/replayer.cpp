/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-present Facebook, Inc. (http://www.facebook.com)  |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include "hphp/runtime/base/replayer.h"

#include <atomic>
#include <fstream>
#include <thread>

#include "hphp/runtime/base/array-data.h"
#include "hphp/runtime/base/array-iterator.h"
#include "hphp/runtime/base/collections.h"
#include "hphp/runtime/base/execution-context.h"
#include "hphp/runtime/base/object-data.h"
#include "hphp/runtime/base/object-iterator.h"
#include "hphp/runtime/base/php-globals.h"
#include "hphp/runtime/base/req-root.h"
#include "hphp/runtime/base/string-data.h"
#include "hphp/runtime/base/tv-comparisons.h"
#include "hphp/runtime/base/type-object.h"
#include "hphp/runtime/base/type-resource.h"
#include "hphp/runtime/base/typed-value.h"
#include "hphp/runtime/base/variable-unserializer.h"
#include "hphp/runtime/ext/asio/asio-external-thread-event.h"
#include "hphp/runtime/ext/asio/asio-session.h"
#include "hphp/runtime/ext/asio/ext_static-wait-handle.h"
#include "hphp/runtime/ext/asio/ext_wait-handle.h"
#include "hphp/runtime/ext/collections/ext_collections-pair.h"
#include "hphp/runtime/ext/collections/ext_collections-vector.h"
#include "hphp/runtime/ext/collections/hash-collection.h"
#include "hphp/runtime/vm/class.h"
#include "hphp/runtime/vm/debugger-hook.h"
#include "hphp/runtime/vm/native.h"
#include "hphp/util/assertions.h"
#include "hphp/util/blob-encoder.h"
#include "hphp/util/build-info.h"

namespace HPHP {

Replayer::~Replayer() {
  always_assert_flog(m_nativeCalls.empty(), "{}", m_nativeCalls.size());
}

String Replayer::file(const String& path) const {
  return m_files.at(path.toCppString());
}

Replayer& Replayer::get() {
  static Replayer replayer;
  return replayer;
}

String Replayer::init(const String& path) {
  std::ifstream ifs{path.data(), std::ios::binary | std::ios::ate};
  always_assert_flog(!ifs.fail(), "{}", path.toCppString());
  std::vector<char> data(static_cast<std::size_t>(ifs.tellg()));
  ifs.seekg(0).read(data.data(), data.size());
  BlobDecoder decoder{data.data(), data.size()};
  const ArrayData* ptr;
  BlobEncoderHelper<decltype(ptr)>::serde(decoder, ptr, false);
  const Array recording{const_cast<ArrayData*>(ptr)};
  const auto header{recording[String{"header"}].asCArrRef()};
  const auto compilerId{header[String{"compilerId"}].asCStrRef()};
  // TODO Enable compiler ID assertion after replay completes successfully.
  // always_assert_flog(compilerId == HPHP::compilerId(), "{}", compilerId);
  m_serverGlobal = header[String{"serverGlobal"}].asCArrRef();
  for (auto i{recording[String{"eteOrder"}].asCArrRef().begin()}; i; ++i) {
    m_externalThreadEventOrder.emplace_back(i.second().asInt64Val());
  }
  for (auto i{recording[String{"files"}].asCArrRef().begin()}; i; ++i) {
    m_files[i.first().asCStrRef().data()] = i.second().asCStrRef().data();
  }
  std::unordered_map<std::uintptr_t, std::uintptr_t> nativeFuncRecordToReplayId;
  for (auto i{recording[String("nativeFuncIds")].asCArrRef().begin()}; i; ++i) {
    const auto id{m_nativeFuncNames[i.first().asCStrRef().data()]};
    nativeFuncRecordToReplayId[i.second().asInt64Val()] = id;
  }
  for (auto i{recording[String{"nativeCalls"}].asCArrRef().begin()}; i; ++i) {
    const auto call{i.second().asCArrRef()};
    always_assert_flog(call.size() == 6, "{}", call.size());
    m_nativeCalls.push_back(NativeCall{
      nativeFuncRecordToReplayId[call[0].asInt64Val()],
      call[1].asCArrRef(),
      call[2].asCArrRef(),
      call[3].asCStrRef(),
      call[4].asCStrRef(),
      static_cast<std::uintptr_t>(call[5].asInt64Val()),
    });
  }
  return header[String{"entryPoint"}].toString();
}

void Replayer::requestInit() const {
  always_assert(HPHP::DebuggerHook::attach<DebuggerHook>());
}

void Replayer::setDebuggerHook(HPHP::DebuggerHook* debuggerHook) {
  m_debuggerHook = debuggerHook;
}

struct Replayer::ExternalThreadEvent final : public AsioExternalThreadEvent {
  ~ExternalThreadEvent() override {
    if (thread.joinable()) {
      thread.join();
    }
  }

  static Object create(const Object& exception) {
    return create(new ExternalThreadEvent{exception});
  }

  static Object create(const TypedValue& result) {
    return create(new ExternalThreadEvent{result});
  }

  void unserialize(TypedValue& result) override {
    if (exception) {
      // NOLINTNEXTLINE(facebook-hte-ThrowNonStdExceptionIssue)
      throw req::root<Object>(exception);
    } else {
      result = this->result;
    }
  }

 private:
  explicit ExternalThreadEvent(const Object& exc) : exception{exc} {}
  explicit ExternalThreadEvent(const TypedValue& res) : result{res} {}

  static Object create(ExternalThreadEvent* event) {
    static std::uint64_t create{0};
    const auto process{Replayer::get().m_externalThreadEventOrder[create++]};
    const auto queue{AsioSession::Get()->getExternalThreadEventQueue()};
    event->thread = std::thread{[event, process, queue] {
      static std::atomic<std::uint64_t> nextToProcess{0};
      while (nextToProcess != process) {
        nextToProcess.wait(nextToProcess);
      }
      while (queue->getQueue() != K_CONSUMER_WAITING) {
        std::this_thread::yield();
      }
      event->markAsFinished();
      nextToProcess++;
      nextToProcess.notify_all();
    }};
    return Object{event->getWaitHandle()};
  }

  Object exception;
  TypedValue result;
  std::thread thread;
};

struct Replayer::DebuggerHook final : public HPHP::DebuggerHook {
  static DebuggerHook* GetInstance() {
    static DebuggerHook hook;
    return &hook;
  }

  void onRequestInit() override {
    auto& replayer{Replayer::get()};
    const Variant serverGlobal{replayer.m_serverGlobal.detach()};
    php_global_set(StaticString{"_SERVER"}, serverGlobal);
    HPHP::DebuggerHook::detach();
    if (auto debuggerHook{replayer.m_debuggerHook}; debuggerHook != nullptr) {
      HPHP::DebuggerHook::s_activeHook = debuggerHook;
      HPHP::DebuggerHook::s_numAttached++;
      RI().m_debuggerHook = debuggerHook;
      RI().m_reqInjectionData.setDebuggerAttached(true);
      RI().m_reqInjectionData.setDebuggerIntr(true);
      debuggerHook->onRequestInit();
    }
  }
};

template<>
Variant Replayer::unserialize(const String& recordedValue) {
  TmpAssign _{RO::EvalCheckPropTypeHints, 0};
  return VariableUnserializer{
    recordedValue.data(),
    static_cast<std::size_t>(recordedValue.size()),
    VariableUnserializer::Type::DebuggerSerialize
  }.unserialize();
}

template<>
void Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  always_assert_flog(variant.isNull(), "{}", recordedValue);
}

template<>
bool Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  always_assert_flog(variant.isBoolean(), "{}", recordedValue);
  return variant.asBooleanVal();
}

template<>
double Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  always_assert_flog(variant.isDouble(), "{}", recordedValue);
  return variant.asDoubleVal();
}

template<>
std::int64_t Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  always_assert_flog(variant.isInteger(), "{}", recordedValue);
  return variant.asInt64Val();
}

template<>
Array Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  always_assert_flog(variant.isArray(), "{}", recordedValue);
  return variant.asCArrRef();
}

template<>
Object Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  if (variant.isNull()) {
    return Object{};
  } else {
    always_assert_flog(variant.isObject(), "{}", recordedValue);
    return variant.asCObjRef();
  }
}

template<>
Resource Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  always_assert_flog(variant.isResource(), "{}", recordedValue);
  return variant.asCResRef();
}

template<>
String Replayer::unserialize(const String& recordedValue) {
  const auto variant{unserialize<Variant>(recordedValue)};
  always_assert_flog(variant.isString(), "{}", recordedValue);
  return variant.asCStrRef();
}

template<>
TypedValue Replayer::unserialize(const String& recordedValue) {
  return unserialize<Variant>(recordedValue).detach();
}

Object Replayer::makeWaitHandle(const NativeCall& call) {
  Object exc;
  TypedValue ret;
  if (!call.exc.empty()) {
    exc = unserialize<Object>(call.exc);
  } else {
    ret = unserialize<TypedValue>(call.ret);
  }
  const auto kind{call.waitHandle - 1};
  switch (static_cast<c_Awaitable::Kind>(kind)) {
    case c_Awaitable::Kind::ExternalThreadEvent:
      if (exc) {
        return ExternalThreadEvent::create(exc);
      } else {
        return ExternalThreadEvent::create(ret);
      }
    case c_Awaitable::Kind::Static:
      if (exc) {
        return Object{c_StaticWaitHandle::CreateFailed(exc.detach())};
      } else {
        return Object{c_StaticWaitHandle::CreateSucceeded(ret)};
      }
    default:
      always_assert_flog(false, "Unhandled WH kind: {}", kind);
  }
}

template<>
void Replayer::nativeArg(const String& recordedArg, const Variant& arg) {
  const auto str{Recorder::serialize<Variant>(arg)};
  always_assert_flog(recordedArg.equal(str), "{} != {}", recordedArg, str);
}

template<>
void Replayer::nativeArg(const String& recordedArg, bool arg) {
  nativeArg<const Variant&>(recordedArg, arg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, bool& arg) {
  arg = unserialize<bool>(recordedArg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, double arg) {
  nativeArg<const Variant&>(recordedArg, arg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, double& arg) {
  arg = unserialize<double>(recordedArg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, std::int64_t arg) {
  nativeArg<const Variant&>(recordedArg, arg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, std::int64_t& arg) {
  arg = unserialize<std::int64_t>(recordedArg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, Array& arg) {
  arg = unserialize<Array>(recordedArg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, const Array& arg) {
  nativeArg<const Variant&>(recordedArg, arg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, ArrayArg arg) {
  nativeArg<const Variant&>(recordedArg, Variant{arg.m_px});
}

template<>
void Replayer::nativeArg(const String& recordedArg, const Class* arg) {
  nativeArg<const Variant&>(recordedArg, const_cast<Class*>(arg));
}

namespace {
  // FIXME This is an incomplete attempt to preserve references in nested objects
  void updateObject(ObjectData* from, ObjectData* to) {
    IteratePropMemOrder(
      from,
      [to](Slot slot, const Class::Prop&, tv_rval value) {
        const auto obj{value.val().pobj};
        if (value.type() == DataType::Object && !obj->isCollection()) {
          updateObject(obj, to->propLvalAtOffset(slot).val().pobj);
        } else {
          tvSet(value.tv(), to->propLvalAtOffset(slot));
        }
        return false;
      },
      [to](TypedValue key, TypedValue value) {
        tvSet(value, to->makeDynProp(tvCastToStringData(key)));
        return false;
      }
    );
  }
} // namespace

template<>
void Replayer::nativeArg(const String& recordedArg, ObjectData* arg) {
  const auto object{unserialize<Object>(recordedArg)};
  const auto cls{object->getVMClass()};
  const auto argCls{arg->getVMClass()};
  always_assert_flog(cls == argCls, "{} != {}", cls->name(), argCls->name());
  if (arg->isCollection()) {
    while (collections::getSize(arg) > 0) {
      collections::pop(arg);
    }
    IterateKV(
      collections::asArray(object.get()),
      [arg](const TypedValue& key, const TypedValue& value) {
        switch (arg->collectionType()) {
          case CollectionType::Map:
            collections::set(arg, &key, &value);
            break;
          case CollectionType::Set:
            collections::append(arg, const_cast<TypedValue*>(&key));
            break;
          case CollectionType::Vector:
            collections::append(arg, const_cast<TypedValue*>(&value));
            break;
          case CollectionType::ImmMap:
          case CollectionType::ImmSet:
          case CollectionType::ImmVector:
          case CollectionType::Pair:
            always_assert(tvEqual(collections::at(arg, &key).tv(), value));
            break;
        }
        return false;
      }
    );
  } else {
    updateObject(object.get(), arg);
  }
  // FIXME This is failing
  // always_assert(object->equal(*arg));
}

template<>
void Replayer::nativeArg(const String& recordedArg, Object& arg) {
  nativeArg<ObjectData*>(recordedArg, arg.get());
}

template<>
void Replayer::nativeArg(const String& recordedArg, const Object& arg) {
  nativeArg<ObjectData*>(recordedArg, arg.get());
}

template<>
void Replayer::nativeArg(const String& recordedArg, ObjectArg arg) {
  nativeArg<ObjectData*>(recordedArg, arg.m_px);
}

template<>
void Replayer::nativeArg(const String& recordedArg, const Resource& arg) {
  nativeArg<const Variant&>(recordedArg, arg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, String& arg) {
  arg = unserialize<String>(recordedArg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, const String& arg) {
  nativeArg<const Variant&>(recordedArg, arg);
}

template<>
void Replayer::nativeArg(const String& recordedArg, StringArg arg) {
  nativeArg<const Variant&>(recordedArg, Variant{arg.m_px});
}

template<>
void Replayer::nativeArg(const String& recordedArg, TypedValue arg) {
  nativeArg<const Variant&>(recordedArg, Variant::wrap(arg));
}

template<>
void Replayer::nativeArg(const String& recordedArg, Variant& arg) {
  arg = unserialize<Variant>(recordedArg);
}

NativeCall Replayer::popNativeCall(std::uintptr_t id) {
  if (m_nativeCalls.empty()) {
    for (const auto& [name, id_] : m_nativeFuncNames) {
      if (id_ == id) {
        always_assert_flog(!m_nativeCalls.empty(), "{}", name);
      }
    }
  }
  const auto call{m_nativeCalls.front()};
  m_nativeCalls.pop_front();
  if (call.id != id) {
    std::string recordedName, replayedName;
    for (const auto& [name, id_] : m_nativeFuncNames) {
      if (id_ == id) {
        replayedName = name;
      }
      if (id_ == call.id) {
        recordedName = name;
      }
    }
    always_assert_flog(call.id == id, "{} != {}", recordedName, replayedName);
  }
  for (auto i{call.stdouts.begin()}; i; ++i) {
    g_context->write(i.second().asCStrRef());
  }
  return call;
}

} // namespace HPHP
