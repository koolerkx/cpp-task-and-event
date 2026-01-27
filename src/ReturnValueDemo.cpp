#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "CoroTask.hpp"
#include "Task.hpp"
#include "TaskAwaiter.hpp"
#include "ThreadPool.hpp"


// Custom struct for demo
struct AssetInfo {
  std::string name;
  size_t size;
  int version;
};


// Demo 1: Basic integer return value
CoroTask<void> BasicIntReturnDemo(ThreadPool& pool) {
  std::cout << "=== Basic Int Return Demo ===\n";

  auto task = std::make_shared<Task<int>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Computing result...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 42;  // The answer to everything
  });

  task->TrySchedule(pool);

  int result = co_await TaskAwaiter<int>{task, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Got result: " << result << "\n\n";
}


// Demo 2: String return value
CoroTask<void> StringReturnDemo(ThreadPool& pool) {
  std::cout << "=== String Return Demo ===\n";

  auto task = std::make_shared<Task<std::string>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Loading configuration...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return std::string{"GraphicsSettings: HighQuality, 1920x1080"};
  });

  task->TrySchedule(pool);

  std::string config = co_await TaskAwaiter<std::string>{task, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Config loaded: " << config << "\n\n";
}


// Demo 3: Custom struct return value
CoroTask<void> CustomStructReturnDemo(ThreadPool& pool) {
  std::cout << "=== Custom Struct Return Demo ===\n";

  auto task = std::make_shared<Task<AssetInfo>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Parsing asset metadata...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return AssetInfo{"HeroModel.fbx", 1024 * 1024 * 25, 2};
  });

  task->TrySchedule(pool);

  AssetInfo info = co_await TaskAwaiter<AssetInfo>{task, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Asset info:\n";
  std::cout << "  Name: " << info.name << "\n";
  std::cout << "  Size: " << info.size << " bytes\n";
  std::cout << "  Version: " << info.version << "\n\n";
}


// Demo 4: Sequential data processing (not true chaining, but sequential awaiting)
CoroTask<void> DataPipelineDemo(ThreadPool& pool) {
  std::cout << "=== Data Pipeline Demo ===\n";

  // Stage 1: Read raw data
  auto readTask = std::make_shared<Task<std::string>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Reading raw data...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return std::string{"100,50,30"};
  });

  readTask->TrySchedule(pool);

  // Wait for read to complete and get result
  std::string rawData = co_await TaskAwaiter<std::string>{readTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Raw data: " << rawData << "\n";

  // Stage 2: Parse and sum (using the data from stage 1)
  auto parseTask = std::make_shared<Task<int>>([rawData]() {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Parsing and summing...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // In a real implementation, we would parse rawData here
    // For demo, we'll just return a computed value
    return 180;  // 100 + 50 + 30
  });

  parseTask->TrySchedule(pool);

  int sum = co_await TaskAwaiter<int>{parseTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Final sum: " << sum << "\n\n";
}


// Demo 5: Parallel tasks with different return types
CoroTask<void> ParallelMultiTypeDemo(ThreadPool& pool) {
  std::cout << "=== Parallel Multi-Type Return Demo ===\n";

  auto intTask = std::make_shared<Task<int>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Computing FPS...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    return 60;
  });

  auto stringTask = std::make_shared<Task<std::string>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Getting GPU name...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    return std::string{"NVIDIA RTX 4090"};
  });

  auto doubleTask = std::make_shared<Task<double>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Measuring memory usage...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    return 4.5;  // GB
  });

  // Schedule all tasks in parallel
  intTask->TrySchedule(pool);
  stringTask->TrySchedule(pool);
  doubleTask->TrySchedule(pool);

  // Await all results
  int fps = co_await TaskAwaiter<int>{intTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] FPS: " << fps << "\n";

  std::string gpu = co_await TaskAwaiter<std::string>{stringTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] GPU: " << gpu << "\n";

  double memory = co_await TaskAwaiter<double>{doubleTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Memory: " << memory << " GB\n\n";
}


// Demo 6: Mixed void and return value tasks
CoroTask<void> MixedVoidAndValueDemo(ThreadPool& pool) {
  std::cout << "=== Mixed Void and Value Return Demo ===\n";

  // First, initialize (void return)
  auto initTask = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Initializing renderer...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  });

  initTask->TrySchedule(pool);

  co_await TaskAwaiter<void>{initTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Renderer initialized\n";

  // Then, get render target info (returns value)
  auto targetTask = std::make_shared<Task<int>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Querying render target...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 12345;  // Render target ID
  });

  targetTask->TrySchedule(pool);

  int targetId = co_await TaskAwaiter<int>{targetTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] Render target ID: " << targetId << "\n";

  // Finally, cleanup (void return)
  auto cleanupTask = std::make_shared<Task<void>>([] {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Cleanup complete\n";
  });

  cleanupTask->TrySchedule(pool);

  co_await TaskAwaiter<void>{cleanupTask, pool};
  std::cout << "[Thread " << std::this_thread::get_id() << "] All done!\n\n";
}


void RunAllReturnValueDemos() {
  std::cout << "==============================================\n";
  std::cout << "=== Task<T> Return Value Demo Suite ===\n";
  std::cout << "==============================================\n\n";

  {
    ThreadPool pool;
    auto coro = BasicIntReturnDemo(pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  {
    ThreadPool pool;
    auto coro = StringReturnDemo(pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  {
    ThreadPool pool;
    auto coro = CustomStructReturnDemo(pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  {
    ThreadPool pool;
    auto coro = DataPipelineDemo(pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  {
    ThreadPool pool;
    auto coro = ParallelMultiTypeDemo(pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }

  {
    ThreadPool pool;
    auto coro = MixedVoidAndValueDemo(pool);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }

  std::cout << "=== All return value demos completed ===\n";
}
