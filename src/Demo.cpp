#include "Task.hpp"
#include "ThreadPool.hpp"
#include <chrono>
#include <iostream>
#include <thread>

void RunBasicDemo() {
  std::cout << "=== Basic Task Execution Demo ===" << std::endl;

  ThreadPool pool;
  auto task1 = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task 1 executed\n";
  });
  auto task2 = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task 2 executed\n";
  });
  auto task3 = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task 3 executed\n";
  });

  task1->TrySchedule(pool);
  task2->TrySchedule(pool);
  task3->TrySchedule(pool);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::cout << std::endl;
}

void RunDAGDemo() {
  std::cout << "=== DAG Dependencies Demo ===" << std::endl;
  std::cout << "Graph: A -> C, B -> C\n" << std::endl;

  ThreadPool pool;

  auto taskA = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task A: Loading Mesh...\n";
  });
  auto taskB = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task B: Loading Texture...\n";
  });
  auto taskC = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task C: Initializing Material (requires A and B)\n";
  });

  taskA->Then(taskC);
  taskB->Then(taskC);

  std::cout << "Scheduling tasks A and B...\n";
  taskA->TrySchedule(pool);
  taskB->TrySchedule(pool);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::cout << std::endl;
}

void RunComplexDAGDemo() {
  std::cout << "=== Complex DAG Demo ===" << std::endl;
  std::cout << "Graph:\n";
  std::cout << "     A\n";
  std::cout << "    / \\\n";
  std::cout << "   B   C\n";
  std::cout << "    \\ /\n";
  std::cout << "     D\n";
  std::cout << "     |\n";
  std::cout << "     E\n" << std::endl;

  ThreadPool pool;

  auto taskA = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task A: Initialize Engine\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });
  auto taskB = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task B: Load Scene Graph\n";
  });
  auto taskC = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task C: Load Shaders\n";
  });
  auto taskD = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task D: Build Render Pipeline (requires B and C)\n";
  });
  auto taskE = std::make_shared<Task>([] {
    std::cout << "[Thread " << std::this_thread::get_id()
              << "] Task E: Start Render Loop (requires D)\n";
  });

  taskA->Then(taskB);
  taskA->Then(taskC);
  taskB->Then(taskD);
  taskC->Then(taskD);
  taskD->Then(taskE);

  std::cout << "Scheduling root task A...\n";
  taskA->TrySchedule(pool);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::cout << std::endl;
}

void RunAllDemo() {
  RunBasicDemo();
  RunDAGDemo();
  RunComplexDAGDemo();
  std::cout << "=== All demos completed ===" << std::endl;
}
