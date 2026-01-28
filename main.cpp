void RunAllDemo();
void RunAllCoroutineDemos();
void RunAllReturnValueDemos();
void RunAllExceptionHandlingDemos();
void RunAllCancellationDemos();

namespace ThenSuccessDemo {
void RunAll();
}

int main() {
    RunAllDemo();
    RunAllCoroutineDemos();
    RunAllReturnValueDemos();
    RunAllExceptionHandlingDemos();
    RunAllCancellationDemos();
    ThenSuccessDemo::RunAll();
    return 0;
}