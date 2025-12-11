/*
 * This source file is part of an OSTIS project. For the latest info, see http://ostis.net
 * Distributed under the MIT License
 * (See accompanying file COPYING.MIT or copy at http://opensource.org/licenses/MIT)
 */

#pragma once

#include "../memory_test.hpp"

extern "C"
{
#include "sc-store/sc-transaction/sc_transaction.h"
#include "sc-store/sc-transaction/sc_memory_transaction_manager.h"
#include "sc-store/sc-transaction/sc_memory_transaction_operations.h"
}

// =============== СОЗДАНИЕ УЗЛА ===============

// Создание узла БЕЗ транзакции
class TestNodeNewDirect : public TestMemory
{
public:
  void Run()
  {
    m_ctx->GenerateNode(ScType::ConstNode);
  }
};

// Создание узла С транзакцией
class TestNodeNewTransaction : public TestMemory
{
public:
  void Run()
  {
    sc_transaction * txn = sc_memory_transaction_new(m_ctx->GetRealContext());
    (void)sc_memory_transaction_node_new(txn, sc_type_const_node);
    sc_memory_transaction_commit(txn);
  }
};

// =============== СОЗДАНИЕ ДУГИ ===============

// Создание дуги БЕЗ транзакции
class TestArcNewDirect : public TestMemory
{
public:
  void Setup(size_t objectsNum) override
  {
    m_node1 = m_ctx->GenerateNode(ScType::ConstNode);
    m_node2 = m_ctx->GenerateNode(ScType::ConstNode);
  }

  void Run()
  {
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, m_node1, m_node2);
  }

private:
  ScAddr m_node1, m_node2;
};

// Создание дуги С транзакцией
class TestArcNewTransaction : public TestMemory
{
public:
  void Setup(size_t objectsNum) override
  {
    m_node1 = m_ctx->GenerateNode(ScType::ConstNode);
    m_node2 = m_ctx->GenerateNode(ScType::ConstNode);
  }

  void Run()
  {
    sc_transaction * txn = sc_memory_transaction_new(m_ctx->GetRealContext());
    sc_addr n1 = m_node1.GetRealAddr();
    sc_addr n2 = m_node2.GetRealAddr();
    (void)sc_memory_transaction_arc_new(txn, sc_type_const_perm_pos_arc, &n1, &n2);
    sc_memory_transaction_commit(txn);
  }

private:
  ScAddr m_node1, m_node2;
};

// =============== УДАЛЕНИЕ ЭЛЕМЕНТА ===============

// Удаление элемента БЕЗ транзакции
class TestElementEraseDirect : public TestMemory
{
public:
  void Run()
  {
    ScAddr node = m_ctx->GenerateNode(ScType::ConstNode);
    m_ctx->EraseElement(node);
  }
};

// Удаление элемента С транзакцией
class TestElementEraseTransaction : public TestMemory
{
public:
  void Run()
  {
    ScAddr node = m_ctx->GenerateNode(ScType::ConstNode);
    sc_transaction * txn = sc_memory_transaction_new(m_ctx->GetRealContext());
    sc_addr addr = node.GetRealAddr();
    sc_memory_transaction_element_free(txn, addr);
    sc_memory_transaction_commit(txn);
  }
};
