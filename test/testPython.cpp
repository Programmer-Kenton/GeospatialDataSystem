/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/28 23:36
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include <python3.8/Python.h>

void callPythonScript() {
    Py_Initialize();
    const char* script = "print('Hello from Python!')";
    PyRun_SimpleString(script);
    Py_Finalize();
}


int main() {
    callPythonScript();
    return 0;
}