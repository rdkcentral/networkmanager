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
