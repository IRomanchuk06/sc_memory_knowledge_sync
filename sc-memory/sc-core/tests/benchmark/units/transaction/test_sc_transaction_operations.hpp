#pragma once

#include "../memory_test.hpp"
#include "sc-store/sc-transaction/sc_memory_transaction_operations.h"

class TestNodeNewDirect : public TestMemory
{
public:
  void Run()
  {
    m_ctx->GenerateNode(ScType::ConstNode);
  }
};

class TestArcNewDirect : public TestMemory
{
private:
  ScAddr m_source;
  ScAddr m_target;
  
public:
  void Initialize()
  {
    TestMemory::Initialize();
    m_source = m_ctx->GenerateNode(ScType::ConstNode);
    m_target = m_ctx->GenerateNode(ScType::ConstNode);
  }
  
  void Run()
  {
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, m_source, m_target);
  }
};

class TestElementEraseDirect : public TestMemory
{
public:
  void Run()
  {
    ScAddr node = m_ctx->GenerateNode(ScType::ConstNode);
    m_ctx->EraseElement(node);
  }
};

class TestNodeNewTransaction : public TestMemory
{
private:
  sc_transaction * m_txn = nullptr;
  
public:
  void InitContext()
  {
    TestMemory::InitContext();
    m_txn = sc_memory_transaction_new(m_ctx->GetRealContext());
  }
  
  void Run()
  {
    sc_memory_transaction_node_new(m_txn, sc_type_const_node);
  }
  
  void DestroyContext()
  {
    if (m_txn != nullptr)
    {
      sc_memory_transaction_commit(m_txn);
      sc_transaction_destroy(m_txn);
      m_txn = nullptr;
    }
    TestMemory::DestroyContext();
  }
};

class TestArcNewTransaction : public TestMemory
{
private:
  sc_transaction * m_txn = nullptr;
  sc_addr m_source_addr;
  sc_addr m_target_addr;
  
public:
  void InitContext()
  {
    TestMemory::InitContext();
    
    ScAddr m_source = m_ctx->GenerateNode(ScType::ConstNode);
    ScAddr m_target = m_ctx->GenerateNode(ScType::ConstNode);
    m_source_addr = m_source.GetRealAddr();
    m_target_addr = m_target.GetRealAddr();
    
    m_txn = sc_memory_transaction_new(m_ctx->GetRealContext());
  }
  
  void Run()
  {
    sc_memory_transaction_arc_new(
      m_txn, 
      sc_type_const_perm_pos_arc, 
      &m_source_addr, 
      &m_target_addr
    );
  }
  
  void DestroyContext()
  {
    if (m_txn != nullptr)
    {
      sc_memory_transaction_commit(m_txn);
      sc_transaction_destroy(m_txn);
      m_txn = nullptr;
    }
    TestMemory::DestroyContext();
  }
};

class TestElementEraseTransaction : public TestMemory
{
private:
  sc_transaction * m_txn = nullptr;
  
public:
  void InitContext()
  {
    TestMemory::InitContext();
    m_txn = sc_memory_transaction_new(m_ctx->GetRealContext());
  }
  
  void Run()
  {
    sc_addr node_addr = sc_memory_transaction_node_new(m_txn, sc_type_const_node);
    
    if (!SC_ADDR_IS_EMPTY(node_addr))
    {
      sc_memory_transaction_element_free(m_txn, node_addr);
    }
  }
  
  void DestroyContext()
  {
    if (m_txn != nullptr)
    {
      sc_memory_transaction_commit(m_txn);
      sc_transaction_destroy(m_txn);
      m_txn = nullptr;
    }
    TestMemory::DestroyContext();
  }
};
