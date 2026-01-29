void RunAllDemo();
void RunAllCoroutineDemos();
void RunAllReturnValueDemos();
void RunAllExceptionHandlingDemos();
void RunAllCancellationDemos();

namespace ThenSuccessDemo {
void RunAll();
}

namespace EventBusDemo {
void RunAll();
}

namespace TypeSafeEventDemo {
void RunAll();
}

namespace CollisionFilterDemo {
void RunAll();
}

int main() {
  RunAllDemo();
  RunAllCoroutineDemos();
  RunAllReturnValueDemos();
  RunAllExceptionHandlingDemos();
  RunAllCancellationDemos();
  ThenSuccessDemo::RunAll();
  EventBusDemo::RunAll();
  TypeSafeEventDemo::RunAll();
  CollisionFilterDemo::RunAll();
  return 0;
}
