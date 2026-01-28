#include <iostream>
#include <stdexcept>
#include <string>

#include "Task.hpp"
#include "ThreadPool.hpp"

namespace ThenSuccessDemo {

// Tests basic success chain with Then semantics
// Shows: sequential execution only when all predecessors succeed
void TestBasicSuccess() {
  std::cout << "\n=== Test 1: Basic Success Chain ===\n";

  ThreadPool pool(4);
  std::string execution_log;

  auto taskA = std::make_shared<Task<int>>([]() -> int {
    std::cout << "Task A executing\n";
    return 42;
  });

  auto taskB = std::make_shared<Task<int>>([&execution_log]() -> int {
    std::cout << "Task B executing\n";
    execution_log += "B ";
    return 100;
  });

  auto taskC = std::make_shared<Task<int>>([&execution_log]() -> int {
    std::cout << "Task C executing\n";
    execution_log += "C ";
    return 200;
  });

  taskA->Then(taskB);
  taskB->Then(taskC);

  taskA->TrySchedule(pool);
  taskC->Wait();

  std::cout << "Execution log: " << execution_log << "\n";
  std::cout << "Task C result: " << taskC->GetResult() << "\n";
}

// Tests exception propagation through Then chain
// Shows: exception prevents subsequent tasks from running
void TestExceptionPropagation() {
  std::cout << "\n=== Test 2: Exception Propagation ===\n";

  ThreadPool pool(4);
  std::string execution_log;

  auto taskA = std::make_shared<Task<int>>([]() -> int {
    std::cout << "Task A executing and throwing\n";
    throw std::runtime_error("Task A failed");
    return 42;
  });

  auto taskB = std::make_shared<Task<int>>([&execution_log]() -> int {
    std::cout << "Task B executing (should NOT see this)\n";
    execution_log += "B ";
    return 100;
  });

  auto taskC = std::make_shared<Task<int>>([&execution_log]() -> int {
    std::cout << "Task C executing (should NOT see this)\n";
    execution_log += "C ";
    return 200;
  });

  taskA->Then(taskB);
  taskB->Then(taskC);

  taskA->TrySchedule(pool);
  taskC->Wait();

  std::cout << "Execution log (should be empty): [" << execution_log << "]\n";

  try {
    taskC->GetResult();
    std::cout << "ERROR: Should have thrown exception\n";
  } catch (const std::runtime_error& e) {
    std::cout << "Caught expected exception: " << e.what() << "\n";
  }
}

// Tests mixing Then and Finally semantics
// Shows: Finally runs regardless of exception, Then only on success
void TestMixedSemantics() {
  std::cout << "\n=== Test 3: Mixed Then and ThenSuccess ===\n";

  ThreadPool pool(4);
  std::string execution_log;

  auto taskA = std::make_shared<Task<int>>([]() -> int {
    std::cout << "Task A executing and throwing\n";
    throw std::runtime_error("Task A failed");
    return 42;
  });

  auto taskB = std::make_shared<Task<int>>([&execution_log]() -> int {
    std::cout << "Task B executing (Then, should still run)\n";
    execution_log += "B ";
    return 100;
  });

  auto taskC = std::make_shared<Task<int>>([&execution_log]() -> int {
    std::cout << "Task C executing (ThenSuccess, should NOT run)\n";
    execution_log += "C ";
    return 200;
  });

  taskA->Finally(taskB);
  taskA->Then(taskC);

  taskA->TrySchedule(pool);
  taskB->Wait();
  taskC->Wait();

  std::cout << "Execution log (should contain 'B' only): [" << execution_log << "]\n";

  try {
    taskC->GetResult();
    std::cout << "ERROR: Should have thrown exception\n";
  } catch (const std::runtime_error& e) {
    std::cout << "Task C caught exception: " << e.what() << "\n";
  }
}

// Tests Then with multiple predecessors, some failing
// Shows: any predecessor failure prevents dependent task
void TestMultiplePredecessors() {
  std::cout << "\n=== Test 4: Multiple Predecessors with Exception ===\n";

  ThreadPool pool(4);

  auto taskA = std::make_shared<Task<int>>([]() -> int {
    std::cout << "Task A executing successfully\n";
    return 42;
  });

  auto taskB = std::make_shared<Task<int>>([]() -> int {
    std::cout << "Task B executing and throwing\n";
    throw std::runtime_error("Task B failed");
    return 100;
  });

  auto taskC = std::make_shared<Task<int>>([]() -> int {
    std::cout << "Task C executing (should NOT see this)\n";
    return 200;
  });

  taskA->Then(taskC);
  taskB->Then(taskC);

  taskA->TrySchedule(pool);
  taskB->TrySchedule(pool);
  taskC->Wait();

  try {
    taskC->GetResult();
    std::cout << "ERROR: Should have thrown exception\n";
  } catch (const std::runtime_error& e) {
    std::cout << "Task C caught exception from B: " << e.what() << "\n";
  }
}

// Tests exception propagation in void tasks
// Shows: void tasks also propagate exceptions through Then
void TestVoidTaskPropagation() {
  std::cout << "\n=== Test 5: Void Task Exception Propagation ===\n";

  ThreadPool pool(4);
  std::string execution_log;

  auto taskA = std::make_shared<Task<void>>([]() {
    std::cout << "Task A (void) executing and throwing\n";
    throw std::runtime_error("Void task failed");
  });

  auto taskB = std::make_shared<Task<void>>([&execution_log]() {
    std::cout << "Task B (void) executing (should NOT see this)\n";
    execution_log += "B ";
  });

  auto taskC = std::make_shared<Task<void>>([&execution_log]() {
    std::cout << "Task C (void) executing (should NOT see this)\n";
    execution_log += "C ";
  });

  taskA->Then(taskB);
  taskB->Then(taskC);

  taskA->TrySchedule(pool);
  taskC->Wait();

  std::cout << "Execution log (should be empty): [" << execution_log << "]\n";
}

// Tests long chain of Then dependencies
// Shows: sequential execution over many stages
void TestLongChain() {
  std::cout << "\n=== Test 6: Long Success Chain ===\n";

  ThreadPool pool(4);
  int counter = 0;

  auto task1 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 1 executing\n";
    counter++;
    return 1;
  });

  auto task2 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 2 executing\n";
    counter++;
    return 2;
  });

  auto task3 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 3 executing\n";
    counter++;
    return 3;
  });

  auto task4 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 4 executing\n";
    counter++;
    return 4;
  });

  auto task5 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 5 executing\n";
    counter++;
    return 5;
  });

  task1->Then(task2);
  task2->Then(task3);
  task3->Then(task4);
  task4->Then(task5);

  task1->TrySchedule(pool);
  task5->Wait();

  std::cout << "Counter (should be 5): " << counter << "\n";
  std::cout << "Task 5 result: " << task5->GetResult() << "\n";
}

// Tests long chain with early failure
// Shows: early failure prevents entire remaining chain
void TestLongChainWithFailure() {
  std::cout << "\n=== Test 7: Long Chain with Early Failure ===\n";

  ThreadPool pool(4);
  int counter = 0;

  auto task1 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 1 executing\n";
    counter++;
    return 1;
  });

  auto task2 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 2 executing and throwing\n";
    counter++;
    throw std::runtime_error("Task 2 failed");
    return 2;
  });

  auto task3 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 3 executing (should NOT see this)\n";
    counter++;
    return 3;
  });

  auto task4 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 4 executing (should NOT see this)\n";
    counter++;
    return 4;
  });

  auto task5 = std::make_shared<Task<int>>([&counter]() -> int {
    std::cout << "Task 5 executing (should NOT see this)\n";
    counter++;
    return 5;
  });

  task1->Then(task2);
  task2->Then(task3);
  task3->Then(task4);
  task4->Then(task5);

  task1->TrySchedule(pool);
  task5->Wait();

  std::cout << "Counter (should be 2, only task1 and task2): " << counter << "\n";

  try {
    task5->GetResult();
    std::cout << "ERROR: Should have thrown exception\n";
  } catch (const std::runtime_error& e) {
    std::cout << "Task 5 caught exception from task 2: " << e.what() << "\n";
  }
}

// Runs all ThenSuccess demonstration tests
// Shows: comprehensive Then semantics validation
void RunAll() {
  std::cout << "\n========================================\n";
  std::cout << "   ThenSuccess Demonstration Tests   \n";
  std::cout << "========================================\n";

  TestBasicSuccess();
  TestExceptionPropagation();
  TestMixedSemantics();
  TestMultiplePredecessors();
  TestVoidTaskPropagation();
  TestLongChain();
  TestLongChainWithFailure();

  std::cout << "\n========================================\n";
  std::cout << "   All ThenSuccess Tests Completed   \n";
  std::cout << "========================================\n";
}

}  // namespace ThenSuccessDemo
