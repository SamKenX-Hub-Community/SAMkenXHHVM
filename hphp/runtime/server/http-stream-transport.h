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

#pragma once

#include "hphp/runtime/server/stream-transport.h"

#include "hphp/util/assertions.h"

#include <folly/io/IOBuf.h>

namespace HPHP {

struct Transport;

namespace stream_transport {
// HttpStreamServerTransport uses non-blocking POST messages and
// chuncked responses to implement a StreamTransport for the server.
// A non-blocking POST message contains a custom NonBlockingPost header.
struct HttpStreamServerTransport final : StreamTransport {
  explicit HttpStreamServerTransport(Transport* t) : m_transport(t) {}
  ~HttpStreamServerTransport() override {}

  void write(folly::StringPiece slice) override;

  void close() override;
  void closeNow() override { }
  bool isClosed() const override { return m_eom_sent; }

  bool isReady() const {
    return m_onData != nullptr;
  }

  void setOnData(OnDataType callback) override;
  void setOnClose(OnCloseType callback) override {
    assertx(!callback || !m_onClose);
    m_onClose = callback;
  }

  void doOnData(std::unique_ptr<folly::IOBuf> buf) {
    if (m_onData) {
      m_onData(std::move(buf));
    }
  }

  void doOnClose() {
    if (m_onClose) {
      m_onClose();
    }
  }

private:
  Transport* m_transport;
  bool m_eom_sent{false};
  OnDataType m_onData;
  OnCloseType m_onClose;
};
} // namespace stream_transport
} // namespace HPHP
