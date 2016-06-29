#include "pyext.h"
#include <stdint.h>
#include <Python.h>
#include "ptp.h"

extern PTPParams* globalparams;

static PyObject * sony_wrapper(PyObject * self, PyObject * args){
    char * input;
    char * result;
    PyObject * ret;
    
    // parse arg
    if (!PyArg_ParseTuple(args, "s", &input)) {
        return NULL;
    }
    
    // run the func
    result = sony(input);
    
    // build the resulting string into pyobj
    ret = PyString_FromString(result);
    //free(result);
    
    return ret;
}

static PyMethodDef SonyMethods[] = {
    {"sony", sony_wrapper, METH_VARARGS, "Say hello" },
    {NULL, NULL, 0, NULL}
};

DL_EXPORT(void) initsony(void)
{
    Py_InitModule("sony", SonyMethods);
}