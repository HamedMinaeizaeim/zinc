#include <Python.h>
#include "computed_variable/computed_variable.h"

staticforward PyTypeObject CmissVariableType;

/* Internal definition */
#define _CmissVariable_check(object)  ((object)->ob_type == &CmissVariableType)

typedef struct {
    PyObject_HEAD
    struct Cmiss_variable *variable;
} CmissVariableObject;

/* Object Methods */

static PyObject*
CmissVariable_get_variable_cpointer(PyObject* self, PyObject* args)
{
	CmissVariableObject *cmiss_variable;
	PyObject *object, *return_code;

	printf("CmissVariable_get_variable_cpointer\n");

	if (_CmissVariable_check(self))
	{
		cmiss_variable = (CmissVariableObject *)self;
		return_code = PyCObject_FromVoidPtr(cmiss_variable->variable, NULL);
	}
	else
	{
		return_code = NULL;
	}

	return(return_code);
}

static PyObject*
CmissVariable_evaluate(PyObject* self, PyObject* args)
{
	CmissVariableObject *cmiss_variable, *variable_in_list;
	int i, number_of_value_variables;
	PyObject *value, *value_cpointer, *value_module, *value_type_module, *value_variable_list,
		*variable, *variable_cpointer, *return_code;
	struct Cmiss_value *new_value, *value_ptr;
	struct Cmiss_variable *variable_ptr;
	struct Cmiss_variable_value *variable_value;
	struct LIST(Cmiss_variable_value) *values;

	printf("CmissVariable_evaluate\n");

	new_value = (struct Cmiss_value *)NULL;
	return_code = (PyObject *)NULL;

	if (!(value_module = PyImport_ImportModule("Cmiss.Value.Value")))
	{
		PyErr_SetString(PyExc_AttributeError, "Unable to import Cmiss.Value.Value module");
		return NULL;
	}

	if (_CmissVariable_check(self))
	{
		cmiss_variable = (CmissVariableObject *)self;

		if (!PyArg_ParseTuple(args,"O:evaluate", &value_variable_list)) 
			return NULL;

		if (!PyList_Check(value_variable_list))
		{
			PyErr_SetString(PyExc_AttributeError, "Argument must be a list");
			return NULL;
		}

		number_of_value_variables = PyList_Size(value_variable_list);
		if (number_of_value_variables && !(number_of_value_variables % 2))
		{
			if (values=CREATE_LIST(Cmiss_variable_value)())
			{
				i=0;
				while ((i<number_of_value_variables)&&values)
				{
					variable = PySequence_GetItem(value_variable_list, i);
					value = PySequence_GetItem(value_variable_list, i + 1);

					if (!((variable_cpointer = PyObject_CallMethod(variable, "get_variable_cpointer", (char *)NULL)) &&
							 PyCObject_Check(variable_cpointer)))
					{
						PyErr_SetString(PyExc_AttributeError, "Unable to extract variable pointer from variable.");
						return NULL;
					}
					variable_ptr = PyCObject_AsVoidPtr(variable_cpointer);

					if (!((value_cpointer = PyObject_CallMethod(value, "get_value_cpointer", (char *)NULL)) &&
							 PyCObject_Check(value_cpointer)))
					{
						PyErr_SetString(PyExc_AttributeError, "Unable to extract value pointer from value.");
						return NULL;
					}
					value_ptr = PyCObject_AsVoidPtr(value_cpointer);
			
					if (variable_value=CREATE(Cmiss_variable_value)(
						variable_ptr, value_ptr))
					{
						if (ADD_OBJECT_TO_LIST(Cmiss_variable_value)(variable_value,values))
						{
							i += 2;
						}
						else
						{
							DESTROY(Cmiss_variable_value)(&variable_value);
							DESTROY_LIST(Cmiss_variable_value)(&values);
						}
					}
					else
					{
						DESTROY_LIST(Cmiss_variable_value)(&values);
					}
				}
				if (new_value = CREATE(Cmiss_value)())
				{
					ACCESS(Cmiss_value)(new_value);
					if (variable_value=CREATE(Cmiss_variable_value)(cmiss_variable->variable,new_value))
					{
						if (!Cmiss_variable_evaluate(variable_value,values))
						{
							DEACCESS(Cmiss_value)(&new_value);
						}
						DESTROY(Cmiss_variable_value)(&variable_value);
					}
					else
					{
						DEACCESS(Cmiss_value)(&new_value);
					}
				}
				DESTROY_LIST(Cmiss_variable_value)(&values);
			}
			if (new_value)
			{
				char *class_string,*type_id_string;
				
				class_string=(char *)NULL;
				if (type_id_string=Cmiss_value_get_type_id_string(new_value))
				{
					if (class_string=(char *)malloc(strlen(type_id_string)+15))
					{
						strcpy(class_string,"Cmiss.Value.");
						strcat(class_string,type_id_string);
					}
				}
				if (class_string)
				{
					printf ("Creating a result in class %s\n", class_string);
					/* Load the module of the class that we want to cast the pointer into */
					if (value_module = PyImport_ImportModule(class_string))
					{
						return_code  = PyObject_CallMethod(value_module, "wrap", "O",
							PyCObject_FromVoidPtr(new_value, NULL));
					}
					else
					{
						PyErr_SetString(PyExc_AttributeError, "Unable to import class string module");
					}
					free(class_string);
				}
			}
		}
		else
		{
			PyErr_SetString(PyExc_AttributeError, "Must be an even number of objects in Value Variable list.");
		}
	}

	return(return_code);
}

static struct PyMethodDef CmissVariable_methods[] =
	{
		{"get_variable_cpointer", CmissVariable_get_variable_cpointer, 1},
		{"evaluate", CmissVariable_evaluate, 1},
		{NULL, NULL, 0}
	};

/* Type Methods */

static PyObject*
CmissVariable_new(PyObject* self, PyObject* args)
{
	char *name;
	CmissVariableObject *cmiss_variable;

	if (!PyArg_ParseTuple(args,"s:new", &name)) 
		return NULL;

	cmiss_variable = PyObject_New(CmissVariableObject, &CmissVariableType);
	if (cmiss_variable->variable = CREATE(Cmiss_variable)((struct Cmiss_variable_package *)NULL,
		name))
	{
		ACCESS(Cmiss_variable)(cmiss_variable->variable);
	}

	printf("Creating new CmissVariable\n");

	return (PyObject*)cmiss_variable;
}

static void
CmissVariable_dealloc(PyObject* self)
{
   CmissVariableObject *cmiss_variable;
 	if (_CmissVariable_check(self))
	{
		cmiss_variable = (CmissVariableObject *)self;
		DEACCESS(Cmiss_variable)(&cmiss_variable->variable);
	}
	PyObject_Del(self);
}

static PyObject *
CmissVariable_getattr(PyObject *self, char *name)
{
	return Py_FindMethod(CmissVariable_methods, (PyObject *)self, name);
}

static PyObject*
CmissVariable_check(PyObject* self, PyObject* args)
{
	PyObject *object, *return_code;

	if (!PyArg_ParseTuple(args,"O:check", &object)) 
		return NULL;

	printf("Checking CmissVariable\n");

	if (_CmissVariable_check(object))
	{
		return_code = PyInt_FromLong(1);
	}
	else
	{
		return_code = PyInt_FromLong(0);
	}

	return(return_code);
}

static PyObject*
CmissVariable_wrap(PyObject* self, PyObject* args)
{
	char *name;
	CmissVariableObject *cmiss_variable;
	PyObject *cmiss_variable_cpointer;

	if (!(PyArg_ParseTuple(args,"O:wrap", &cmiss_variable_cpointer)
		&& PyCObject_Check(cmiss_variable_cpointer)))
	{
		PyErr_SetString(PyExc_AttributeError, "Incorrect argument for wrap function.");
		return NULL;			 
	}

	cmiss_variable = PyObject_New(CmissVariableObject, &CmissVariableType);
	if (!(cmiss_variable->variable = ACCESS(Cmiss_variable)(
		(Cmiss_variable_id)PyCObject_AsVoidPtr(cmiss_variable_cpointer))))
	{
		PyErr_SetString(PyExc_AttributeError, "Unable to extract Cmiss.Variable pointer.");
		return NULL;			 
	}

	printf("Wrapping CmissVariable\n");

	return (PyObject*)cmiss_variable;
}

static PyObject *
CmissVariable_repr(PyObject* self)
{
	char *name;
	CmissVariableObject *cmiss_variable;
	PyObject *string;

	string = (PyObject *)NULL;
 	if (_CmissVariable_check(self))
	{		
		cmiss_variable = (CmissVariableObject *)self;
		if (get_name_Cmiss_variable(cmiss_variable->variable, &name))
		{
			string = PyString_FromString(name);
			free(name);
		}
	}
	else
	{
		PyErr_SetString(PyExc_AttributeError, "self is not a Cmiss.Value.Derivativematrix");
	}
	return (string);
}

static PyTypeObject CmissVariableType = {
    PyObject_HEAD_INIT(NULL)
    0,
    "Variable",
    sizeof(CmissVariableObject),
    0,
    CmissVariable_dealloc, /*tp_dealloc*/
    0,          /*tp_print*/
    CmissVariable_getattr,          /*tp_getattr*/
    0,          /*tp_setattr*/
    0,          /*tp_compare*/
    CmissVariable_repr,          /*tp_repr*/
    0,          /*tp_as_number*/
    0,          /*tp_as_sequence*/
    0,          /*tp_as_mapping*/
    0,          /*tp_hash */
};

static PyMethodDef CmissVariableType_methods[] = {
    {"new", CmissVariable_new, METH_VARARGS,
     "Create a new Cmiss Variable object."},
    {"check", CmissVariable_check, METH_VARARGS,
     "Check if object is of type Cmiss Variable object."},
    {"wrap", CmissVariable_wrap, METH_VARARGS,
     "Wrap a C CmissVariable in a python Cmiss Variable object."},
    {NULL, NULL, 0, NULL}
};

DL_EXPORT(void)
initVariable(void) 
{
	CmissVariableType.ob_type = &PyType_Type;
	
	printf ("In initVariable\n");

	Py_InitModule("Variable", CmissVariableType_methods);
}
