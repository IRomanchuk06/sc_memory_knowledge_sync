#include "sc_memory_transaction_manager.h"

#include "sc-store/sc_storage.h"
#include "sc-store/sc-base/sc_condition_private.h"
#include "sc-store/sc-base/sc_monitor_table.h"
#include "sc-store/sc-base/sc_monitor_table_private.h"
#include "sc-store/sc-base/sc_thread.h"
#include "sc-store/sc-container/sc_struct_node.h"

#include <sc-core/sc-base/sc_allocator.h>

sc_memory_transaction_manager * transaction_manager = null_ptr;

sc_result sc_memory_transaction_manager_initialize(sc_memory_transaction_manager * manager)
{
  if (sc_memory_transaction_manager_is_initialized())
  {
    return SC_RESULT_OK;
  }

  transaction_manager = manager;

  if (transaction_manager == null_ptr)
  {
    return SC_RESULT_ERROR_INVALID_PARAMS;
  }

  transaction_manager->should_stop = SC_FALSE;
  transaction_manager->threads = null_ptr;
  transaction_manager->txn_count = 0;

  transaction_manager->transaction_queue = sc_mem_new(sc_queue, 1);
  if (transaction_manager->transaction_queue == null_ptr)
  {
    return SC_RESULT_ERROR_FULL_MEMORY;
  }
  sc_queue_init(transaction_manager->transaction_queue);

  transaction_manager->mutex = sc_mem_new(sc_mutex, 1);
  if (transaction_manager->mutex == null_ptr)
  {
    goto error_cleanup_queue;
  }
  sc_mutex_init(transaction_manager->mutex);

  transaction_manager->queue_condition = sc_mem_new(sc_condition, 1);
  if (transaction_manager->queue_condition == null_ptr)
  {
    goto error_cleanup_mutex;
  }
  sc_cond_init(transaction_manager->queue_condition);

  transaction_manager->monitor_table = sc_mem_new(sc_monitor_table, 1);
  if (transaction_manager->monitor_table == null_ptr)
  {
    goto error_cleanup_condition;
  }
  _sc_monitor_table_init(transaction_manager->monitor_table);

  transaction_manager->processed_elements =
      sc_hash_table_init(sc_hash_table_default_hash_func, sc_hash_table_default_equal_func, null_ptr, null_ptr);
  if (transaction_manager->processed_elements == null_ptr)
  {
    goto error_cleanup_monitor_table;
  }

  if (sc_list_init(&transaction_manager->threads) != SC_RESULT_OK)
  {
    goto error_cleanup_hash_table;
  }

  for (int i = 0; i < SC_TRANSACTION_THREAD_COUNT; ++i)
  {
    sc_thread * thread = sc_thread_new("sc_transaction_handler", sc_transaction_handler, null_ptr);
    if (thread == null_ptr)
    {
      goto error_cleanup_threads;
    }
    sc_list_push_back(transaction_manager->threads, thread);
  }

  return SC_RESULT_OK;

error_cleanup_threads:
  if (transaction_manager->threads != null_ptr)
  {
    transaction_manager->should_stop = SC_TRUE;
    sc_cond_broadcast(transaction_manager->queue_condition);

    for (sc_struct_node const * it = transaction_manager->threads->begin; it != null_ptr; it = it->next)
    {
      sc_thread * thread = it->data;
      if (thread != null_ptr)
      {
        sc_thread_join(thread);
        sc_thread_unref(thread);
      }
    }
    sc_list_destroy(transaction_manager->threads);
    transaction_manager->threads = null_ptr;
  }

error_cleanup_hash_table:
  sc_hash_table_destroy(transaction_manager->processed_elements);
  transaction_manager->processed_elements = null_ptr;

error_cleanup_monitor_table:
  sc_mem_free(transaction_manager->monitor_table);
  transaction_manager->monitor_table = null_ptr;

error_cleanup_condition:
  sc_cond_destroy(transaction_manager->queue_condition);
  sc_mem_free(transaction_manager->queue_condition);

error_cleanup_mutex:
  sc_mutex_destroy(transaction_manager->mutex);
  sc_mem_free(transaction_manager->mutex);

error_cleanup_queue:
  sc_queue_destroy(transaction_manager->transaction_queue);
  sc_mem_free(transaction_manager->transaction_queue);

  transaction_manager = null_ptr;

  return SC_RESULT_ERROR;
}

sc_bool sc_memory_transaction_manager_is_initialized()
{
  return transaction_manager != null_ptr;
}

void sc_memory_transaction_shutdown()
{
  if (transaction_manager != null_ptr)
  {
    transaction_manager->should_stop = SC_TRUE;
    sc_cond_broadcast(transaction_manager->queue_condition);
    sc_transaction_manager_destroy();
  }
}

sc_memory_transaction_manager * sc_memory_transaction_manager_get()
{
  return transaction_manager;
}

void sc_transaction_manager_destroy()
{
  if (transaction_manager == null_ptr)
    return;

  transaction_manager->should_stop = SC_TRUE;

  sc_cond_broadcast(transaction_manager->queue_condition);

  if (transaction_manager->threads != null_ptr)
  {
    for (sc_struct_node const * it = transaction_manager->threads->begin; it != null_ptr; it = it->next)
    {
      if (it->data != null_ptr)
      {
        sc_thread * thread = it->data;
        sc_thread_join(thread);
        sc_thread_unref(thread);
      }
    }
    sc_list_destroy(transaction_manager->threads);
    transaction_manager->threads = null_ptr;
  }

  if (transaction_manager->transaction_queue != null_ptr)
  {
    sc_queue_destroy(transaction_manager->transaction_queue);
    sc_mem_free(transaction_manager->transaction_queue);
    transaction_manager->transaction_queue = null_ptr;
  }

  if (transaction_manager->queue_condition != null_ptr)
  {
    sc_cond_destroy(transaction_manager->queue_condition);
    sc_mem_free(transaction_manager->queue_condition);
    transaction_manager->queue_condition = null_ptr;
  }

  if (transaction_manager->mutex != null_ptr)
  {
    sc_mutex_destroy(transaction_manager->mutex);
    sc_mem_free(transaction_manager->mutex);
    transaction_manager->mutex = null_ptr;
  }

  if (transaction_manager->monitor_table != null_ptr)
  {
    sc_mem_free(transaction_manager->monitor_table);
    transaction_manager->monitor_table = null_ptr;
  }

  if (transaction_manager->processed_elements != null_ptr)
  {
    sc_hash_table_destroy(transaction_manager->processed_elements);
    transaction_manager->processed_elements = null_ptr;
  }

  sc_mem_free(transaction_manager);
  transaction_manager = null_ptr;
}

void sc_transaction_manager_transaction_add(sc_transaction * txn)
{
  sc_mutex_lock(transaction_manager->mutex);

  sc_queue_push(transaction_manager->transaction_queue, txn);

  sc_cond_signal(transaction_manager->queue_condition);

  sc_mutex_unlock(transaction_manager->mutex);
}

void sc_transaction_manager_block_elements(sc_hash_table * elements)
{
  sc_mutex_lock(transaction_manager->mutex);

  sc_hash_table_iterator iter;
  sc_hash_table_iterator_init(&iter, elements);
  void *key, *value;

  while (sc_hash_table_iterator_next(&iter, &key, &value))
  {
    sc_hash_table_insert(transaction_manager->processed_elements, key, (void *)1);
  }

  sc_mutex_unlock(transaction_manager->mutex);
}

void sc_transaction_manager_unblock_elements(sc_hash_table * elements)
{
  sc_mutex_lock(transaction_manager->mutex);

  sc_hash_table_iterator iter;
  sc_hash_table_iterator_init(&iter, elements);
  void *key, *value;

  while (sc_hash_table_iterator_next(&iter, &key, &value))
  {
    sc_hash_table_remove(transaction_manager->processed_elements, key);
  }

  sc_mutex_unlock(transaction_manager->mutex);
}

static sc_bool sc_transaction_elements_intersect_unsafe(sc_transaction const * txn, sc_hash_table * processed_elements)
{
  if (txn == null_ptr || txn->elements == null_ptr || processed_elements == null_ptr)
    return SC_FALSE;

  sc_hash_table * smaller_table =
      sc_hash_table_size(txn->elements) <= sc_hash_table_size(processed_elements) ? txn->elements : processed_elements;
  sc_hash_table * larger_table = smaller_table == txn->elements ? processed_elements : txn->elements;

  sc_hash_table_iterator iter;
  sc_hash_table_iterator_init(&iter, smaller_table);
  void *key, *value;

  while (sc_hash_table_iterator_next(&iter, &key, &value))
  {
    if (sc_hash_table_get(larger_table, key) != null_ptr)
    {
      return SC_TRUE;
    }
  }

  return SC_FALSE;
}

sc_bool sc_transaction_elements_intersect(sc_transaction const * txn, sc_hash_table * processed_elements)
{
  sc_mutex_lock(transaction_manager->mutex);
  sc_bool result = sc_transaction_elements_intersect_unsafe(txn, processed_elements);
  sc_mutex_unlock(transaction_manager->mutex);
  return result;
}

void sc_transaction_manager_transaction_execute(sc_transaction * txn)
{
  if (txn == null_ptr)
    return;

  sc_mutex_lock(transaction_manager->mutex);
  sc_bool has_intersection = sc_transaction_elements_intersect_unsafe(txn, transaction_manager->processed_elements);
  sc_mutex_unlock(transaction_manager->mutex);

  if (!has_intersection)
  {
    sc_transaction_manager_block_elements(txn->elements);

    if (sc_transaction_validate(txn))
    {
      sc_transaction_apply(txn);
      txn->state = SC_TRANSACTION_COMMITTED;
    }
    else
    {
      txn->state = SC_TRANSACTION_FAILED;
    }

    sc_transaction_manager_unblock_elements(txn->elements);
  }
  else
  {
    txn->state = SC_TRANSACTION_FAILED;
  }
}

void * sc_transaction_handler(void * data)
{
  (void)data;

  while (1)
  {
    sc_mutex_lock(transaction_manager->mutex);

    if (transaction_manager->should_stop)
    {
      sc_mutex_unlock(transaction_manager->mutex);
      break;
    }

    while (transaction_manager->transaction_queue->size == 0 && !transaction_manager->should_stop)
    {
      sc_cond_wait(transaction_manager->queue_condition, transaction_manager->mutex);
    }

    if (transaction_manager->should_stop)
    {
      sc_mutex_unlock(transaction_manager->mutex);
      break;
    }

    sc_transaction * txn = null_ptr;
    if (transaction_manager->transaction_queue->size > 0)
    {
      txn = sc_queue_front(transaction_manager->transaction_queue);
      sc_queue_pop(transaction_manager->transaction_queue);
    }

    sc_mutex_unlock(transaction_manager->mutex);

    if (txn != null_ptr)
    {
      sc_transaction_manager_transaction_execute(txn);
      txn->state = SC_TRANSACTION_EXECUTED;
    }
  }

  return null_ptr;
}
