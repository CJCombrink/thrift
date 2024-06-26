#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements. See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership. The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the License for the
# specific language governing permissions and limitations
# under the License.
#

THRIFT_FILES = $(wildcard ../*.thrift)

ERL_FLAG = erl
# make sure ThriftTest.thrift is generated last to prevent conflicts with other *.thrift files
.generated: $(THRIFT_FILES)
	for f in $(THRIFT_FILES) ; do \
	  $(THRIFT) --gen $(ERL_FLAG) -o src $$f ; \
	done ; \
	$(THRIFT) --gen $(ERL_FLAG) -o src ../v0.16/ThriftTest.thrift
	touch .generated

precross: .generated
	$(REBAR) compile

maintainer-clean-local:
	$(RM) -r ebin/

clean:
	$(REBAR) clean
	$(RM)    .generated
	$(RM) -r .rebar/
	$(RM) -r src/gen-erl/

distdir:
	$(MAKE) $(AM_MAKEFLAGS) distdir-am

dist-hook:
	$(RM)    $(distdir)/.generated
	$(RM) -r $(distdir)/.rebar/
	$(RM) -r $(distdir)/ebin/
	$(RM) -r $(distdir)/src/gen-erl/
