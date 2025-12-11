#include "sc_transaction_buffer.h"

#include "sc-store/sc_storage_private.h"
#include "sc-store/sc-base/sc_monitor_table.h"
#include "sc-store/sc-container/sc_pair.h"

#include <sc-store/sc-transaction/sc_element_version.h>
#include <sc-core/sc-base/sc_allocator.h>
#include <sc-core/sc-container/sc_list.h>
#include <sc-core/sc_stream.h>

typedef struct sc_content_version
{
  sc_addr addr;
  sc_stream * content;
} sc_content_version;

// Initialize transaction-local buffer that holds all changes until commit/rollback
void sc_transaction_buffer_initialize(sc_transaction_buffer * transaction_buffer, sc_uint64 txn_id)
{
  if (transaction_buffer == null_ptr)
    return;

  transaction_buffer->transaction_id = txn_id;

  // Set of newly created elements (keys only, values are unused)
  transaction_buffer->new_elements =
      sc_hash_table_init(sc_hash_table_default_hash_func, sc_hash_table_default_equal_func, null_ptr, null_ptr);
  if (transaction_buffer->new_elements == null_ptr)
    goto error_nothing;

  // Map of modified elements: key = sc_addr (int), value = snapshot of sc_element_data
  transaction_buffer->modified_elements =
      sc_hash_table_init(sc_hash_table_default_hash_func, sc_hash_table_default_equal_func, null_ptr, sc_mem_free);
  if (transaction_buffer->modified_elements == null_ptr)
    goto error_new_elements;

  // Set of elements scheduled for deletion (keys only)
  transaction_buffer->deleted_elements =
      sc_hash_table_init(sc_hash_table_default_hash_func, sc_hash_table_default_equal_func, null_ptr, null_ptr);
  if (transaction_buffer->deleted_elements == null_ptr)
    goto error_modified_elements;

  // Map of link content updates: key = sc_addr (int), value = sc_stream*
  // Value destroy function must match GDestroyNotify signature
  transaction_buffer->content_changes =
      sc_hash_table_init(sc_hash_table_default_hash_func, sc_hash_table_default_equal_func, null_ptr, sc_stream_free);
  if (transaction_buffer->content_changes == null_ptr)
    goto error_deleted_elements;

  return;

error_deleted_elements:
  sc_hash_table_destroy(transaction_buffer->deleted_elements);
error_modified_elements:
  sc_hash_table_destroy(transaction_buffer->modified_elements);
error_new_elements:
  sc_hash_table_destroy(transaction_buffer->new_elements);
error_nothing:
  return;
}

// Destroy all internal containers inside the buffer; caller owns the buffer object itself
void sc_transaction_buffer_destroy(sc_transaction_buffer * transaction_buffer)
{
  if (transaction_buffer == null_ptr)
    return;

  if (transaction_buffer->new_elements != null_ptr)
  {
    sc_hash_table_destroy(transaction_buffer->new_elements);
    transaction_buffer->new_elements = null_ptr;
  }

  if (transaction_buffer->modified_elements != null_ptr)
  {
    sc_hash_table_destroy(transaction_buffer->modified_elements);
    transaction_buffer->modified_elements = null_ptr;
  }

  if (transaction_buffer->deleted_elements != null_ptr)
  {
    sc_hash_table_destroy(transaction_buffer->deleted_elements);
    transaction_buffer->deleted_elements = null_ptr;
  }

  if (transaction_buffer->content_changes != null_ptr)
  {
    sc_hash_table_destroy(transaction_buffer->content_changes);
    transaction_buffer->content_changes = null_ptr;
  }
}

// Register a newly created element in the buffer (no additional payload needed)
sc_bool sc_transaction_buffer_created_add(sc_transaction_buffer const * buffer, sc_addr const * addr)
{
  if (buffer == null_ptr || buffer->new_elements == null_ptr)
    return SC_FALSE;

  if (SC_ADDR_IS_EMPTY(*addr))
    return SC_FALSE;

  // Store integer-encoded local address as key
  void * key = (void *)(uintptr_t)SC_ADDR_LOCAL_TO_INT(*addr);
  sc_hash_table_insert(buffer->new_elements, key, null_ptr);

  return SC_TRUE;
}

// Store or update snapshot of modified element data for optimistic validation and versioning
sc_bool sc_transaction_buffer_modified_add(
    sc_transaction_buffer const * buffer,
    sc_addr const * addr,
    sc_element_data const * new_element_data)
{
  if (buffer == null_ptr || buffer->modified_elements == null_ptr)
    return SC_FALSE;

  if (SC_ADDR_IS_EMPTY(*addr))
    return SC_FALSE;

  sc_element * element = null_ptr;
  if (sc_storage_get_element_by_addr(*addr, &element) != SC_RESULT_OK || element == null_ptr)
    return SC_FALSE;

  void * key = (void *)(uintptr_t)SC_ADDR_LOCAL_TO_INT(*addr);

  // If snapshot for this addr already exists, just overwrite its contents
  sc_element_data * old_snapshot = sc_hash_table_get(buffer->modified_elements, key);
  if (old_snapshot != null_ptr)
  {
    *old_snapshot = *new_element_data;
    return SC_TRUE;
  }

  // First modification of this element in this transaction: allocate and store snapshot
  sc_element_data * snapshot = sc_mem_new(sc_element_data, 1);
  if (snapshot == null_ptr)
    return SC_FALSE;
  *snapshot = *new_element_data;

  sc_hash_table_insert(buffer->modified_elements, key, snapshot);

  return SC_TRUE;
}

// Mark element as deleted; actual removal happens when applying the transaction
sc_bool sc_transaction_buffer_removed_add(sc_transaction_buffer const * buffer, sc_addr const * addr)
{
  if (buffer == null_ptr || buffer->deleted_elements == null_ptr)
    return SC_FALSE;

  if (SC_ADDR_IS_EMPTY(*addr))
    return SC_FALSE;

  void * key = (void *)(uintptr_t)SC_ADDR_LOCAL_TO_INT(*addr);
  sc_hash_table_insert(buffer->deleted_elements, key, null_ptr);

  return SC_TRUE;
}

// Buffer content changes for link-like elements; latest value wins for each addr
sc_bool sc_transaction_buffer_content_set(
    sc_transaction_buffer const * buffer,
    sc_addr const * addr,
    sc_stream const * content)
{
  if (buffer == null_ptr || buffer->content_changes == null_ptr)
    return SC_FALSE;

  if (SC_ADDR_IS_EMPTY(*addr))
    return SC_FALSE;

  void * key = (void *)(uintptr_t)SC_ADDR_LOCAL_TO_INT(*addr);

  // Replace previous content for the same addr, if any
  sc_stream * old_content = sc_hash_table_get(buffer->content_changes, key);
  if (old_content != null_ptr)
  {
    sc_stream_free(old_content);
  }

  sc_hash_table_insert(buffer->content_changes, key, (void *)content);

  return SC_TRUE;
}

// Check if given address corresponds to an element created within this transaction
sc_bool sc_transaction_buffer_contains_created(sc_transaction_buffer const * buffer, sc_addr const * addr)
{
  if (buffer == null_ptr || buffer->new_elements == null_ptr)
    return SC_FALSE;

  void * key = (void *)(uintptr_t)SC_ADDR_LOCAL_TO_INT(*addr);
  return sc_hash_table_contains(buffer->new_elements, key);
}
