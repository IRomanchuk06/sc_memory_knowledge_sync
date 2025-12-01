#include "benchmark/benchmark.h"
#include "units/transaction/test_sc_transaction_add_node.hpp"

#include <chrono>
#include <atomic>

template <class BMType>
void BM_MemoryThreaded(benchmark::State & state)
{
  static std::atomic_int ctxNum = {0};
  BMType test;
  
  if (state.thread_index() == 0)
  {
    test.Initialize();
  }

  auto start = std::chrono::high_resolution_clock::now();
  uint32_t iterations = 0;
  
  for (auto _ : state)
  {
    state.PauseTiming();
    if (!test.HasContext())
    {
      test.InitContext();
      ctxNum.fetch_add(1);
    }
    state.ResumeTiming();

    test.Run();
    ++iterations;
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end - start;
  std::stringstream stream;
  stream << state.max_iterations << " " << elapsed.count() << std::endl;
  std::cout << stream.str();
  
  state.counters["rate"] = benchmark::Counter(iterations, benchmark::Counter::kIsRate);
  
  if (state.thread_index() == 0)
  {
    while (ctxNum.load() != 0);
    test.Shutdown();
  }
  else
  {
    test.DestroyContext();
    ctxNum.fetch_add(-1);
  }
}

// =============== СОЗДАНИЕ УЗЛА ===============
// Без транзакции - 1000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewDirect)
->Threads(1)
->Iterations(1000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 1000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewTransaction)
->Threads(1)
->Iterations(1000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// Без транзакции - 500 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewDirect)
->Threads(1)
->Iterations(500)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 500 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewTransaction)
->Threads(1)
->Iterations(500)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== СОЗДАНИЕ ДУГИ ===============
// Без транзакции - 1000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewDirect)
->Threads(1)
->Iterations(1000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 1000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewTransaction)
->Threads(1)
->Iterations(1000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// Без транзакции - 500 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewDirect)
->Threads(1)
->Iterations(500)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 500 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewTransaction)
->Threads(1)
->Iterations(500)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== УДАЛЕНИЕ ЭЛЕМЕНТА ===============
// Без транзакции - 1000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseDirect)
->Threads(1)
->Iterations(1000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 1000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseTransaction)
->Threads(1)
->Iterations(1000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// Без транзакции - 500 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseDirect)
->Threads(1)
->Iterations(500)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 500 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseTransaction)
->Threads(1)
->Iterations(500)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== СОЗДАНИЕ УЗЛА ===============
// Без транзакции - 5000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewDirect)
->Threads(1)
->Iterations(5000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 5000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewTransaction)
->Threads(1)
->Iterations(5000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== СОЗДАНИЕ ДУГИ ===============
// Без транзакции - 5000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewDirect)
->Threads(1)
->Iterations(5000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 5000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewTransaction)
->Threads(1)
->Iterations(5000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== УДАЛЕНИЕ ЭЛЕМЕНТА ===============
// Без транзакции - 5000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseDirect)
->Threads(1)
->Iterations(5000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 5000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseTransaction)
->Threads(1)
->Iterations(5000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== СОЗДАНИЕ УЗЛА ===============
// Без транзакции - 10000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewDirect)
->Threads(1)
->Iterations(10000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 10000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestNodeNewTransaction)
->Threads(1)
->Iterations(10000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== СОЗДАНИЕ ДУГИ ===============
// Без транзакции - 10000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewDirect)
->Threads(1)
->Iterations(10000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 10000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestArcNewTransaction)
->Threads(1)
->Iterations(10000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// =============== УДАЛЕНИЕ ЭЛЕМЕНТА ===============
// Без транзакции - 10000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseDirect)
->Threads(1)
->Iterations(10000)
->Unit(benchmark::TimeUnit::kMicrosecond);

// С транзакцией - 10000 итераций
BENCHMARK_TEMPLATE(BM_MemoryThreaded, TestElementEraseTransaction)
->Threads(1)
->Iterations(10000)
->Unit(benchmark::TimeUnit::kMicrosecond);


BENCHMARK_MAIN();
