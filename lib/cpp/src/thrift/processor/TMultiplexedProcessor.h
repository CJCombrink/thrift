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

#ifndef THRIFT_TMULTIPLEXEDPROCESSOR_H_
#define THRIFT_TMULTIPLEXEDPROCESSOR_H_ 1

#include <thrift/protocol/TProtocolDecorator.h>
#include <thrift/TProcessor.h>

namespace apache {
namespace thrift {
namespace protocol {

/**
 *  To be able to work with any protocol, we needed
 *  to allow them to call readMessageBegin() and get a TMessage in exactly
 *  the standard format, without the service name prepended to TMessage.name.
 */
class StoredMessageProtocol : public TProtocolDecorator {
public:
  StoredMessageProtocol(std::shared_ptr<protocol::TProtocol> _protocol,
                        const std::string& _name,
                        const TMessageType _type,
                        const int32_t _seqid)
    : TProtocolDecorator(_protocol), name(_name), type(_type), seqid(_seqid) {}

  uint32_t readMessageBegin_virt(std::string& _name, TMessageType& _type, int32_t& _seqid) override {

    _name = name;
    _type = type;
    _seqid = seqid;

    return 0; // (Normal TProtocol read functions return number of bytes read)
  }

  std::string name;
  TMessageType type;
  int32_t seqid;
};
} // namespace protocol

/**
 * <code>TMultiplexedProcessor</code> is a <code>TProcessor</code> allowing
 * a single <code>TServer</code> to provide multiple services.
 *
 * <p>To do so, you instantiate the processor and then register additional
 * processors with it, as shown in the following example:</p>
 *
 * <blockquote><code>
 *     std::shared_ptr<TMultiplexedProcessor> processor(new TMultiplexedProcessor());
 *
 *     processor->registerProcessor(
 *         "Calculator",
 *         std::shared_ptr<TProcessor>( new CalculatorProcessor(
 *             std::shared_ptr<CalculatorHandler>( new CalculatorHandler()))));
 *
 *     processor->registerProcessor(
 *         "WeatherReport",
 *         std::shared_ptr<TProcessor>( new WeatherReportProcessor(
 *             std::shared_ptr<WeatherReportHandler>( new WeatherReportHandler()))));
 *
 *     std::shared_ptr<TServerTransport> transport(new TServerSocket(9090));
 *     TSimpleServer server(processor, transport);
 *
 *     server.serve();
 * </code></blockquote>
 */
class TMultiplexedProcessor : public TProcessor {
public:
  typedef std::map<std::string, std::shared_ptr<TProcessor> > services_t;

  /**
    * 'Register' a service with this <code>TMultiplexedProcessor</code>.  This
    * allows us to broker requests to individual services by using the service
    * name to select them at request time.
    *
    * \param [in] serviceName Name of a service, has to be identical to the name
    *                         declared in the Thrift IDL, e.g. "WeatherReport".
    * \param [in] processor   Implementation of a service, usually referred to
    *                         as "handlers", e.g. WeatherReportHandler,
    *                         implementing WeatherReportIf interface.
    */
  void registerProcessor(const std::string& serviceName, std::shared_ptr<TProcessor> processor);

  /**
   * Register a service to be called to process queries without service name
   * \param [in] processor   Implementation of a service.
   */
  void registerDefault(const std::shared_ptr<TProcessor>& processor);

  /**
   * Chew up invalid input and return an exception to throw.
   */
  TException protocol_error(std::shared_ptr<protocol::TProtocol> in,
                            std::shared_ptr<protocol::TProtocol> out,
                            const std::string& name,
                            int32_t seqid,
                            const std::string& msg) const;

  /**
   * This implementation of <code>process</code> performs the following steps:
   *
   * <ol>
   *     <li>Read the beginning of the message.</li>
   *     <li>Extract the service name from the message.</li>
   *     <li>Using the service name to locate the appropriate processor.</li>
   *     <li>Dispatch to the processor, with a decorated instance of TProtocol
   *         that allows readMessageBegin() to return the original TMessage.</li>
   * </ol>
   *
   * \throws TException If the message type is not T_CALL or T_ONEWAY, if
   * the service name was not found in the message, or if the service
   * name was not found in the service map.
   */
  bool process(std::shared_ptr<protocol::TProtocol> in,
               std::shared_ptr<protocol::TProtocol> out,
               void* connectionContext) override;

private:
  /** Map of service processor objects, indexed by service names. */
  services_t services;

  //! If a non-multi client requests something, it goes to the
  //! default processor (if one is defined) for backwards compatibility.
  std::shared_ptr<TProcessor> defaultProcessor;
};
}
}

#endif // THRIFT_TMULTIPLEXEDPROCESSOR_H_
