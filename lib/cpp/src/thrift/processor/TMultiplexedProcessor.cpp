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

#include <thrift/processor/TMultiplexedProcessor.h>
#include <thrift/TApplicationException.h>
#include <thrift/TProcessor.h>

#include <boost/tokenizer.hpp>

namespace apache {
namespace thrift {

void TMultiplexedProcessor::registerProcessor(const std::string& serviceName, std::shared_ptr<TProcessor> processor) {
  services[serviceName] = processor;
}

void TMultiplexedProcessor::registerDefault(const std::shared_ptr<TProcessor>& processor) {
  defaultProcessor = processor;
}

TException TMultiplexedProcessor::protocol_error(std::shared_ptr<protocol::TProtocol> in,
                          std::shared_ptr<protocol::TProtocol> out,
                          const std::string& name,
                          int32_t seqid,
                          const std::string& msg) const {
  in->skip(::apache::thrift::protocol::T_STRUCT);
  in->readMessageEnd();
  in->getTransport()->readEnd();
  ::apache::thrift::TApplicationException
    x(::apache::thrift::TApplicationException::PROTOCOL_ERROR,
      "TMultiplexedProcessor: " + msg);
  out->writeMessageBegin(name, ::apache::thrift::protocol::T_EXCEPTION, seqid);
  x.write(out.get());
  out->writeMessageEnd();
  out->getTransport()->writeEnd();
  out->getTransport()->flush();
  return TException(msg);
}

bool TMultiplexedProcessor::process(std::shared_ptr<protocol::TProtocol> in,
              std::shared_ptr<protocol::TProtocol> out,
              void* connectionContext) {
  std::string name;
  protocol::TMessageType type;
  int32_t seqid;

  // Use the actual underlying protocol (e.g. TBinaryProtocol) to read the
  // message header.  This pulls the message "off the wire", which we'll
  // deal with at the end of this method.
  in->readMessageBegin(name, type, seqid);

  if (type != protocol::T_CALL && type != protocol::T_ONEWAY) {
    // Unexpected message type.
    throw protocol_error(in, out, name, seqid, "Unexpected message type");
  }

  // Extract the service name
  boost::tokenizer<boost::char_separator<char> > tok(name, boost::char_separator<char>(":"));

  std::vector<std::string> tokens;
  std::copy(tok.begin(), tok.end(), std::back_inserter(tokens));

  // A valid message should consist of two tokens: the service
  // name and the name of the method to call.
  if (tokens.size() == 2) {
    // Search for a processor associated with this service name.
    auto it = services.find(tokens[0]);

    if (it != services.end()) {
      std::shared_ptr<TProcessor> processor = it->second;
      // Let the processor registered for this service name
      // process the message.
      return processor
          ->process(std::shared_ptr<protocol::TProtocol>(
                        new protocol::StoredMessageProtocol(in, tokens[1], type, seqid)),
                    out,
                    connectionContext);
    } else {
      // Unknown service.
      throw protocol_error(in, out, name, seqid,
          "Unknown service: " + tokens[0] +
      ". Did you forget to call registerProcessor()?");
    }
  } else if (tokens.size() == 1) {
  if (defaultProcessor) {
      // non-multiplexed client forwards to default processor
      return defaultProcessor
          ->process(std::shared_ptr<protocol::TProtocol>(
                        new protocol::StoredMessageProtocol(in, tokens[0], type, seqid)),
                    out,
                    connectionContext);
  } else {
  throw protocol_error(in, out, name, seqid,
    "Non-multiplexed client request dropped. "
    "Did you forget to call defaultProcessor()?");
  }
  } else {
  throw protocol_error(in, out, name, seqid,
      "Wrong number of tokens.");
  }
}

}
}
