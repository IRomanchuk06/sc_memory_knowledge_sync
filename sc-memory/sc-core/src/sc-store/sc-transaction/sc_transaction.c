#include "sc_transaction.h"

#include "sc_memory_transaction_manager.h"
#include "sc-core/sc_memory.h"
#include "sc-store/sc_element.h"
#include "sc-store/sc_storage_private.h"
#include "sc-store/sc_version_segment.h"
#include "sc-store/sc-container/sc_pair.h"

#include <sc-core/sc-base/sc_allocator.h>

sc_transaction * sc_transaction_new(sc_uint64 const txn_id, sc_memory_context * ctx)
{
  // Allocate transaction object from custom allocator
  sc_transaction * txn = _sc_mem_new(sizeof(sc_transaction));
  if (txn == null_ptr)
    return null_ptr;

  // Transaction must always be associated with a valid context
  if (ctx == null_ptr)
  {
    sc_mem_free(txn);
    return null_ptr;
  }

  txn->transaction_id = txn_id;
  txn->is_committed = SC_FALSE;
  txn->ctx = ctx;
  txn->state = SC_TRANSACTION_PENDING;

  // Per-transaction monitor for synchronization around transaction state and buffer
  txn->monitor = _sc_mem_new(sizeof(sc_monitor));
  if (txn->monitor == null_ptr)
  {
    sc_mem_free(txn);
    return null_ptr;
  }
  sc_monitor_init(txn->monitor);

  // Transaction-local buffer that stores all changes until commit/rollback
  txn->transaction_buffer = _sc_mem_new(sizeof(sc_transaction_buffer));
  if (txn->transaction_buffer == null_ptr)
  {
    sc_monitor_destroy(txn->monitor);
    sc_mem_free(txn->monitor);
    sc_mem_free(txn);
    return null_ptr;
  }
  sc_transaction_buffer_initialize(txn->transaction_buffer, txn_id);

  // Hash-table that tracks all elements touched by this transaction (write set)
  txn->elements =
      sc_hash_table_init(sc_hash_table_default_hash_func, sc_hash_table_default_equal_func, null_ptr, null_ptr);
  if (txn->elements == null_ptr)
  {
    sc_transaction_buffer_destroy(txn->transaction_buffer);
    sc_mem_free(txn->transaction_buffer);
    sc_monitor_destroy(txn->monitor);
    sc_mem_free(txn->monitor);
    sc_mem_free(txn);
    return null_ptr;
  }

  return txn;
}

void sc_transaction_destroy(sc_transaction * txn)
{
  if (txn != null_ptr)
  {
    if (txn->transaction_buffer != null_ptr)
    {
      // Destroy all buffered changes and internal structures
      sc_transaction_buffer_destroy(txn->transaction_buffer);
      sc_mem_free(txn->transaction_buffer);
    }
    if (txn->elements != null_ptr)
    {
      // Only destroys container, not elements in memory
      sc_hash_table_destroy(txn->elements);
    }
    if (txn->monitor != null_ptr)
    {
      sc_monitor_destroy(txn->monitor);
      sc_mem_free(txn->monitor);
    }
    sc_mem_free(txn);
  }
}

// Compare two snapshots of sc_element_data to detect any modification
sc_bool _sc_transaction_validate_data(sc_element_data const * data1, sc_element_data const * data2)
{
  if (data1 == null_ptr || data2 == null_ptr)
    return SC_FALSE;

  if (data1->flags.states != data2->flags.states || data1->first_out_arc.seg != data2->first_out_arc.seg
      || data1->first_out_arc.offset != data2->first_out_arc.offset
      || data1->first_in_arc.seg != data2->first_in_arc.seg || data1->first_in_arc.offset != data2->first_in_arc.offset
      ||
#ifdef SC_OPTIMIZE_SEARCHING_INCOMING_CONNECTORS_FROM_STRUCTURES
      data1->first_in_arc_from_structure.seg != data2->first_in_arc_from_structure.seg
      || data1->first_in_arc_from_structure.offset != data2->first_in_arc_from_structure.offset ||
#endif
      data1->arc.begin.seg != data2->arc.begin.seg || data1->arc.begin.offset != data2->arc.begin.offset
      || data1->arc.end.seg != data2->arc.end.seg || data1->arc.end.offset != data2->arc.end.offset
      || data1->arc.next_begin_out_arc.seg != data2->arc.next_begin_out_arc.seg
      || data1->arc.next_begin_out_arc.offset != data2->arc.next_begin_out_arc.offset
      || data1->arc.prev_begin_out_arc.seg != data2->arc.prev_begin_out_arc.seg
      || data1->arc.prev_begin_out_arc.offset != data2->arc.prev_begin_out_arc.offset
      || data1->arc.next_begin_in_arc.seg != data2->arc.next_begin_in_arc.seg
      || data1->arc.next_begin_in_arc.offset != data2->arc.next_begin_in_arc.offset
      || data1->arc.next_end_out_arc.seg != data2->arc.next_end_out_arc.seg
      || data1->arc.next_end_out_arc.offset != data2->arc.next_end_out_arc.offset
      || data1->arc.next_end_in_arc.seg != data2->arc.next_end_in_arc.seg
      || data1->arc.next_end_in_arc.offset != data2->arc.next_end_in_arc.offset
      || data1->arc.prev_end_in_arc.seg != data2->arc.prev_end_in_arc.seg
      || data1->arc.prev_end_in_arc.offset != data2->arc.prev_end_in_arc.offset ||
#ifdef SC_OPTIMIZE_SEARCHING_INCOMING_CONNECTORS_FROM_STRUCTURES
      data1->arc.prev_in_arc_from_structure.seg != data2->arc.prev_in_arc_from_structure.seg
      || data1->arc.prev_in_arc_from_structure.offset != data2->arc.prev_in_arc_from_structure.offset
      || data1->arc.next_in_arc_from_structure.seg != data2->arc.next_in_arc_from_structure.seg
      || data1->arc.next_in_arc_from_structure.offset != data2->arc.next_in_arc_from_structure.offset ||
#endif
      data1->incoming_arcs_count != data2->incoming_arcs_count
      || data1->outgoing_arcs_count != data2->outgoing_arcs_count)
  {
    return SC_FALSE;
  }

  return SC_TRUE;
}

// Validate that all modified elements in the buffer are still equal to their snapshots
// This is used as optimistic concurrency check before applying the transaction
sc_bool _sc_transaction_validate_modify_elements(sc_hash_table const * elements_table)
{
  if (elements_table == null_ptr)
    return SC_TRUE;

  sc_hash_table_iterator iter;
  void * key;
  void * value;

  sc_hash_table_iterator_init(&iter, (sc_hash_table *)elements_table);
  while (sc_hash_table_iterator_next(&iter, &key, &value))
  {
    sc_addr element_addr;
    // Keys store local integer representation of sc_addr
    SC_ADDR_LOCAL_FROM_INT((uintptr_t)key, element_addr);
    sc_element_data const * snapshot = (sc_element_data const *)value;

    sc_element * element;
    sc_storage_get_element_by_addr(element_addr, &element);

    // Take a fresh snapshot from storage and compare with buffered snapshot
    sc_element_data * current_data = sc_element_data_new();
    sc_storage_get_element_data_by_addr(element_addr, current_data);

    if (!_sc_transaction_validate_data(snapshot, current_data))
    {
      sc_mem_free(current_data);
      return SC_FALSE;
    }

    sc_mem_free(current_data);
  }

  return SC_TRUE;
}

sc_bool sc_transaction_validate(sc_transaction * txn)
{
  // Fast path: transaction without any touched elements is always valid
  if (sc_hash_table_size(txn->elements) == 0)
  {
    return SC_TRUE;
  }

  // Validate only modified elements; created/deleted do not require snapshot comparison
  if (!_sc_transaction_validate_modify_elements(txn->transaction_buffer->modified_elements))
  {
    return SC_FALSE;
  }

  return SC_TRUE;
}

// Apply new versions for all modified elements stored in the transaction buffer
void _sc_transaction_apply_modified_elements(sc_hash_table const * elements_table, sc_uint64 const txn_id)
{
  if (elements_table == null_ptr)
    return;

  sc_hash_table_iterator iter;
  void * key;
  void * value;

  sc_hash_table_iterator_init(&iter, (sc_hash_table *)elements_table);
  while (sc_hash_table_iterator_next(&iter, &key, &value))
  {
    sc_addr element_addr;
    SC_ADDR_LOCAL_FROM_INT((uintptr_t)key, element_addr);

    sc_element * element = null_ptr;
    sc_storage_get_element_by_addr(element_addr, &element);

    sc_element_data const * new_data = (sc_element_data const *)value;

    // Create a new version node in version chain for this element
    sc_element_version * new_version = sc_element_create_new_version(element, new_data, txn_id);

    // Append version into corresponding version segment
    sc_version_segment_add(element->version_history, new_version);
  }
}

// Apply physical deletion of elements scheduled for removal by this transaction
void _sc_transaction_apply_deleted_elements(sc_hash_table const * elements_table, sc_memory_context * ctx)
{
  if (elements_table == null_ptr)
    return;

  sc_hash_table_iterator iter;
  void * key;
  void * value;

  sc_hash_table_iterator_init(&iter, (sc_hash_table *)elements_table);
  while (sc_hash_table_iterator_next(&iter, &key, &value))
  {
    sc_addr element_addr;
    SC_ADDR_LOCAL_FROM_INT((uintptr_t)key, element_addr);

    sc_memory_element_free(ctx, element_addr);
  }
}

// Commit-time entry point: apply all buffered changes to underlying storage
void sc_transaction_apply(sc_transaction const * txn)
{
  _sc_transaction_apply_modified_elements(txn->transaction_buffer->modified_elements, txn->transaction_id);
  _sc_transaction_apply_deleted_elements(txn->transaction_buffer->deleted_elements, txn->ctx);
}

// Currently a no-op placeholder; can be extended to reset local state without destroying transaction
void sc_transaction_clear(sc_transaction * txn) {}

sc_bool sc_transaction_element_new(sc_transaction const * txn, sc_addr const * addr)
{
  if (txn == null_ptr || addr == null_ptr || txn->transaction_buffer == null_ptr)
    return SC_FALSE;

  // Track element in transaction write set; value is unused, only presence matters
  sc_hash_table_insert(txn->elements, (void *)addr, null_ptr);

  // Delegate actual bookkeeping to transaction buffer
  return sc_transaction_buffer_created_add(txn->transaction_buffer, addr);
}

sc_bool sc_transaction_element_change(
    sc_transaction const * txn,
    sc_addr const * addr,
    sc_element_data const * new_data)
{
  if (txn == null_ptr || addr == null_ptr || txn->transaction_buffer == null_ptr)
    return SC_FALSE;

  if (SC_ADDR_IS_EMPTY(*addr))
    return SC_FALSE;

  // Remember that this element is part of transaction write set
  sc_hash_table_insert(txn->elements, (void *)addr, null_ptr);

  // Store new snapshot in buffer for later validation and apply
  return sc_transaction_buffer_modified_add(txn->transaction_buffer, addr, new_data);
}

sc_bool sc_transaction_element_remove(sc_transaction const * txn, sc_addr const * addr)
{
  if (txn == null_ptr || addr == null_ptr || txn->transaction_buffer == null_ptr)
    return SC_FALSE;

  if (SC_ADDR_IS_EMPTY(*addr))
    return SC_FALSE;

  sc_hash_table_insert(txn->elements, (void *)addr, null_ptr);

  // Mark element as deleted in transaction buffer
  return sc_transaction_buffer_removed_add(txn->transaction_buffer, addr);
}

sc_bool sc_transaction_element_content_set(sc_transaction const * txn, sc_addr const * addr, sc_stream const * content)
{
  if (txn == null_ptr || addr == null_ptr || content == null_ptr || txn->transaction_buffer == null_ptr)
    return SC_FALSE;

  if (SC_ADDR_IS_EMPTY(*addr))
    return SC_FALSE;

  sc_hash_table_insert(txn->elements, (void *)addr, null_ptr);

  // Buffer link content updates; actual write happens on apply
  return sc_transaction_buffer_content_set(txn->transaction_buffer, addr, content);
}
