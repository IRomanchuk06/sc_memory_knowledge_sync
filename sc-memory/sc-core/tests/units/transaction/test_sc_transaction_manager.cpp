#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sc-memory/test/sc_test.hpp>

extern "C"
{
#include "sc-store/sc-base/sc_thread.h"
#include <sc-core/sc-base/sc_allocator.h>
#include <sc-store/sc-transaction/sc_memory_transaction_manager.h>
}

class ScTransactionManagerTest : public ScMemoryTest
{
protected:
  sc_memory_transaction_manager * transaction_manager = nullptr;

  void SetUp() override
  {
    ScMemoryTest::SetUp();
    transaction_manager = sc_memory_transaction_manager_get();
    if (!sc_memory_transaction_manager_is_initialized())
    {
      sc_memory_transaction_manager * manager = sc_mem_new(sc_memory_transaction_manager, 1);
      sc_memory_transaction_manager_initialize(manager);
    }
    transaction_manager = sc_memory_transaction_manager_get();
  }

  void TearDown() override
  {
    sc_memory_transaction_shutdown();
    ScMemoryTest::TearDown();
  }
};

// Test Initialization
TEST_F(ScTransactionManagerTest, Initialization)
{
  ASSERT_NE(transaction_manager, nullptr);
  EXPECT_TRUE(sc_memory_transaction_manager_is_initialized());
}

// Test Shutdown
TEST_F(ScTransactionManagerTest, Shutdown)
{
  sc_memory_transaction_shutdown();
  EXPECT_FALSE(sc_memory_transaction_manager_is_initialized());
  EXPECT_EQ(sc_memory_transaction_manager_get(), nullptr);
}
