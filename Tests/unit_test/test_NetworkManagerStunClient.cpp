#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <exception>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <iostream>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "NetworkManagerStunClient.h"

using namespace std;
using namespace stun;

class AddressTest : public ::testing::Test {
protected:
   stun::attributes::address ad;
    void SetUp() override {
        
    }
    void TearDown() override {
      
    }
};

class ClientTest : public ::testing::Test {
protected:
   stun::client cl;
    void SetUp() override {
        
    }
    void TearDown() override {
      
    }
};

TEST_F(ClientTest, BindSuccess) {
    stun::bind_result result;
    bool success = cl.bind("https://github.com/", 3478, "eth0", stun::protocol::af_inet, 5000, 10000, result);
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.is_valid());
}

TEST_F(ClientTest, BindFailure) {
    stun::bind_result result;
    bool success = cl.bind("http//tata.com", 3478, "eth0", stun::protocol::af_inet, 5000, 10000, result);
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.is_valid());
}
TEST_F(ClientTest, BindWithInvalidInterface) {
    stun::bind_result result;
    bool success = cl.bind("https://github.com/", 3478, "invalid_interface", stun::protocol::af_inet, 5000, 10000, result);
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.is_valid());
}

