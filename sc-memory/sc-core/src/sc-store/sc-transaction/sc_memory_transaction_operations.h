#ifndef SC_MEMORY_TRANSACTION_OPERATIONS_H
#define SC_MEMORY_TRANSACTION_OPERATIONS_H

#include "sc_transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

sc_addr sc_memory_transaction_node_new(sc_transaction const * txn, sc_type type);
sc_addr sc_memory_transaction_link_new(sc_transaction const * txn);
sc_addr sc_memory_transaction_arc_new(
    sc_transaction const * txn,
    sc_type type,
    sc_addr * beg_addr,
    sc_addr * end_addr);
sc_addr sc_memory_transaction_element_free(sc_transaction const * txn, sc_addr addr);

sc_transaction * sc_memory_transaction_new(sc_memory_context * ctx);
sc_result sc_memory_transaction_commit(sc_transaction * txn);

#ifdef __cplusplus
}
#endif

#endif  // SC_MEMORY_TRANSACTION_OPERATIONS_H
