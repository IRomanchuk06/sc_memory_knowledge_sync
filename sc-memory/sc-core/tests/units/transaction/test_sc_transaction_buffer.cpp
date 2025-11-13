#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sc-memory/test/sc_test.hpp>

extern "C"
{
#include <sc-store/sc-container/sc_pair.h>
#include <sc-store/sc_storage_private.h>
#include <sc-store/sc-transaction/sc_transaction_buffer.h>
#include <sc-store/sc_element.h>
#include <sc-core/sc_stream.h>
#include <sc-store/sc-container/sc_struct_node.h>
#include <sc-core/sc_stream_memory.h>
#include <sc-core/sc-base/sc_allocator.h>
#include <sc-store/sc-base/sc_monitor_table.h>
}

class ScTransactionBufferTest : public ScMemoryTest
{
protected:
  sc_transaction_buffer* buffer = sc_mem_new(sc_transaction_buffer, 1);

  void SetUp() override
  {
    ScMemoryTest::SetUp();
    sc_transaction_buffer_initialize(buffer, 1);
    ASSERT_NE(buffer, nullptr);
  }

  void TearDown() override
  {
    sc_transaction_buffer_destroy(buffer);
    sc_mem_free(buffer);
    ScMemoryTest::TearDown();
  }
};

TEST_F(ScTransactionBufferTest, InitializationTest)
{
  EXPECT_NE(buffer->new_elements, nullptr);
  EXPECT_NE(buffer->modified_elements, nullptr);
  EXPECT_NE(buffer->deleted_elements, nullptr);
  EXPECT_NE(buffer->content_changes, nullptr);
  EXPECT_EQ(buffer->transaction_id, 1u);
}

TEST_F(ScTransactionBufferTest, CreatedAddTest)
{
  constexpr sc_addr addr = {1, 1};
  EXPECT_TRUE(sc_transaction_buffer_created_add(buffer, &addr));
  EXPECT_TRUE(sc_transaction_buffer_contains_created(buffer, &addr));
}

TEST_F(ScTransactionBufferTest, RemovedAddTest)
{
  constexpr sc_addr addr = {1, 1};
  EXPECT_TRUE(sc_transaction_buffer_removed_add(buffer, &addr));
  
  EXPECT_EQ(sc_hash_table_size(buffer->deleted_elements), 1u);

  sc_uint32 addr_hash = SC_ADDR_LOCAL_TO_INT(addr);
  void * key = (void *)(uintptr_t)addr_hash;
  EXPECT_TRUE(sc_hash_table_contains(buffer->deleted_elements, key));

  EXPECT_TRUE(sc_transaction_buffer_removed_add(buffer, &addr));
  EXPECT_EQ(sc_hash_table_size(buffer->deleted_elements), 1u);
}

TEST_F(ScTransactionBufferTest, ModifiedAddTest)
{
  sc_addr addr;
  sc_storage_allocate_new_element(m_ctx->GetRealContext(), &addr);

  sc_element* element;
  sc_storage_get_element_by_addr(addr, &element);

  element->incoming_arcs_count++;
  sc_element_data const new_data = {
      .flags = element->flags,
      .first_out_arc = element->first_out_arc,
      .first_in_arc = element->first_in_arc,
  #ifdef SC_OPTIMIZE_SEARCHING_INCOMING_CONNECTORS_FROM_STRUCTURES
      .first_in_arc_from_structure = element->first_in_arc_from_structure,
  #endif
      .arc = element->arc,
      .incoming_arcs_count = element->incoming_arcs_count,
      .outgoing_arcs_count = element->outgoing_arcs_count
  };

  EXPECT_TRUE(sc_transaction_buffer_modified_add(buffer, &addr, &new_data));
  
  EXPECT_EQ(sc_hash_table_size(buffer->modified_elements), 1u);

  sc_uint32 const addr_hash = SC_ADDR_LOCAL_TO_INT(addr);
  void * key = (void *)(uintptr_t)addr_hash;
  sc_element_data * stored_data = static_cast<sc_element_data*>(sc_hash_table_get(buffer->modified_elements, key));
  
  EXPECT_NE(stored_data, nullptr);
  EXPECT_EQ(stored_data->incoming_arcs_count, new_data.incoming_arcs_count);
}

TEST_F(ScTransactionBufferTest, ContentSetTest)
{
  constexpr sc_addr addr = {1, 4};
  sc_stream* stream = sc_stream_memory_new("test", 4, SC_STREAM_FLAG_READ, SC_FALSE);
  ASSERT_NE(stream, nullptr);

  EXPECT_TRUE(sc_transaction_buffer_content_set(buffer, &addr, stream));
  
  EXPECT_EQ(sc_hash_table_size(buffer->content_changes), 1u);

  constexpr sc_uint32 addr_hash = SC_ADDR_LOCAL_TO_INT(addr);
  void * key = (void *)(uintptr_t)addr_hash;
  sc_stream * stored_stream = static_cast<sc_stream*>(sc_hash_table_get(buffer->content_changes, key));
  
  EXPECT_NE(stored_stream, nullptr);
  EXPECT_EQ(stored_stream, stream);

  sc_stream* new_stream = sc_stream_memory_new("new_test", 8, SC_STREAM_FLAG_READ, SC_FALSE);
  EXPECT_TRUE(sc_transaction_buffer_content_set(buffer, &addr, new_stream));
  
  EXPECT_EQ(sc_hash_table_size(buffer->content_changes), 1u);

  stored_stream = static_cast<sc_stream*>(sc_hash_table_get(buffer->content_changes, key));
  EXPECT_NE(stored_stream, nullptr);
  EXPECT_EQ(stored_stream, new_stream);
}