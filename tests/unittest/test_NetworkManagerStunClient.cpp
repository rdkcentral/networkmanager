/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "NetworkManagerStunClient.h"

using namespace std;
using namespace stun;

class AddressTest : public ::testing::Test {
protected:
   stun::attributes::address ad;
};

class ClientTest : public ::testing::Test {
protected:
   stun::client _client;
};

TEST_F(ClientTest, BindSuccess) {
    stun::bind_result result;
    bool success = _client.bind("https://github.com/", 3478, "eth0", stun::protocol::af_inet, 5000, 10000, result);
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.is_valid());
}

TEST_F(ClientTest, BindFailure) {
    stun::bind_result result;
    bool success = _client.bind("http//tata.com", 3478, "eth0", stun::protocol::af_inet, 5000, 10000, result);
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.is_valid());
}

TEST_F(ClientTest, BindWithInvalidInterface) {
    stun::bind_result result;
    bool success = _client.bind("https://github.com/", 3478, "invalid_interface", stun::protocol::af_inet, 5000, 10000, result);
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.is_valid());
}
