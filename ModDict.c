#ifndef PY_SSIZE_T_CLEAN
#define PY_SSIZE_T_CLEAN
#endif /* PY_SSIZE_T_CLEAN */

#include <Python.h>

#if PY_VERSION_HEX >= 0x03090000
#include <genericaliasobject.h>
#endif
#ifndef Py_TPFLAGS_MAPPING
#define Py_TPFLAGS_MAPPING  0
#endif

#define MODDICT_USE_SUPER       0
#define MODDICT_USE_LONGOBJECT  1

/*
 *
 */

#ifndef false
#define false  0
#endif
#ifndef true
#define true   1
#endif

typedef _Bool bool;

/*
 *
 */

#if !defined(NDEBUG) && (_DEBUG || DEBUG)
#define MODDICT_DEBUG  1
#define MODDICT_PRINT(x)  printf x
#define MODDICT_LOG(x)    ({ printf("%s(%d): ", __FILE__, __LINE__); printf x; putchar('\n'); })
#else
#define MODDICT_DEBUG  0
#define MODDICT_PRINT(x)
#define MODDICT_LOG(x)
#endif

#define UNUSED(x)  ((void)x)

inline static int
IsNull(PyObject *object)
{
    return !object || (object == Py_None);
}

#define SetNull(p) SetNULL((void **) (p))
inline static void
SetNULL(void **buffer)
{
    void *p = *buffer;
    buffer = NULL;

    PyMem_Free(p);
}

inline static PyObject *
IncRef(PyObject *object)
{
    Py_INCREF(object);
    return object;
}

inline static void
ClearObject(PyObject **object)
{
    PyObject *p = *object;

    *object = NULL;
    Py_XDECREF(p);
}

inline static PyObject *
SetObject(PyObject **oldobj, PyObject *newobj)
{
    PyObject *old = *oldobj;

    Py_INCREF(newobj);
    Py_XDECREF(old);
    *oldobj = newobj;
    return newobj;
}

inline static PyObject *
SetNone(PyObject **oldobj)
{
    return SetObject(oldobj, Py_None);
}

inline static PyObject *
NewNone()
{
    return IncRef(Py_None);
}

inline static PyObject *
NewBool(int value)
{
    return IncRef(value ? Py_True : Py_False);
}

inline static PyObject *
SetKeyError(PyObject *key)
{
    PyErr_SetObject(PyExc_KeyError, key);
    return NULL;
}

/*
 * Object: ModDict
 */

typedef struct ModDictObject {
    PyObject_HEAD
    PyObject *dict;
    digit divisor;
    PyObject *divisor_;
    PyObject *keys;
    PyObject *values;
    digit *remainder;
    digit *rem_keys;
} ModDictObject;

enum {
    MODDICT_KEY_ERROR = -2,
    MODDICT_KEY_FAILED = -1,
};

typedef struct ModDictKeyInfo {
    digit key;
    digit rem;
} ModDictKeyInfo;

typedef uint8_t fdivcnt_t;

/* ******** */

static PyTypeObject *get_moddict_type();
#define ModDictType (get_moddict_type())


static int
ModDictKeyInfo_Compare(const void *a, const void *b)
{
    return (((const ModDictKeyInfo *) a)->key -
            ((const ModDictKeyInfo *) b)->key);
}

static int64_t
ModDict_find_divisor(digit divmax, Py_ssize_t dict_size, ModDictKeyInfo *keys)
{
    Py_ssize_t mod_gbsize, mod_gblen;
    digit mod_gbstep = (1 << 10);
    digit mod_gbcount, divgbcnt;
    fdivcnt_t *mod_gbuf, *mod_gbnew;
    digit divisor;
    bool injective;

    Py_ssize_t key_pos, key_mark, key_last;
    digit key, rem;

    mod_gbcount = (dict_size + mod_gbstep - 1) / mod_gbstep;
    mod_gblen = mod_gbcount * mod_gbstep;
    mod_gbsize = mod_gblen * sizeof(fdivcnt_t);
    if (!(mod_gbuf = PyMem_Malloc(mod_gbsize)))
        return -1;
    memset(mod_gbuf, 0, mod_gbsize);

    key_mark = -1;
    key_last = 0;
    for (divisor = (digit) dict_size; divisor <= divmax; divisor++) {
        divgbcnt = divisor / mod_gbstep;
        if (divgbcnt >= mod_gbcount) {
            mod_gbcount = divgbcnt + 1;
            mod_gblen = mod_gbcount * mod_gbstep;
            mod_gbsize = mod_gblen * sizeof(fdivcnt_t);
            if (!(mod_gbnew = PyMem_Realloc(mod_gbuf, mod_gbsize))) {
                PyMem_Free(mod_gbuf);
                return -1;
            }
            mod_gbuf = mod_gbnew;
            memset(mod_gbuf + (mod_gblen - mod_gbstep), 0, mod_gbstep * sizeof(fdivcnt_t));
        }

        for (key_pos = key_mark + 1; key_pos < key_last; key_pos++)
            mod_gbuf[keys[key_pos].rem] = 0;

        injective = true;
        for (key_pos = key_mark + 1; key_pos < dict_size; key_pos++) {
            key_last = key_pos;
            key = keys[key_pos].key;
            rem = key % divisor;
            keys[key_pos].rem = rem;
            if (mod_gbuf[rem]) {
                injective = false;
                break;
            }
            mod_gbuf[rem] = 1;
            if (key < divisor)
                key_mark = key_pos;
        }
        if (injective)
            break;
    }

    PyMem_Free(mod_gbuf);
    return injective ? divisor : 0;
}

static int
ModDict_create_table(ModDictObject *self, PyObject *dict)
{
    PyObject *dict_keys = NULL, *dict_vals = NULL;
    PyObject *mod_keys = NULL, *mod_vals = NULL;
    PyObject *idict = NULL, *key = NULL, *val = NULL;
    PyObject *divisor_ = NULL;

    digit divisor = 0, divmax, digmax;
    digit *key_table = NULL;
    digit *mod_table = NULL;
    uint8_t *mod_gbuf = NULL;
    digit *remainder = NULL;
    digit *rem_keys = NULL;

    ModDictKeyInfo *dict_keyidx = NULL;

    Py_ssize_t dict_size = 0;
    Py_ssize_t dict_pos, rem_pos, key_pos;
    long long key_num;
    int overflow;
    int64_t fdivisor;
    int res = -1;

    digmax = 0xffffffff;

    SetNone(&self->dict);
    self->divisor = 0;
    SetNone(&self->divisor_);
    SetNone(&self->keys);
    SetNone(&self->values);
    SetNull(&self->remainder);
    SetNull(&self->rem_keys);

    if (!(dict_size = PyDict_Size(dict))) {
        if (!(self->dict = PyDict_New()))
            goto error;
        return 0;
    }
    if (dict_size > digmax)
        goto type_error;
    if (!(dict_keys = PyTuple_New(dict_size)))
        goto error;
    if (!(dict_vals = PyTuple_New(dict_size)))
        goto error;
    if (!(dict_keyidx = PyMem_Malloc(dict_size * sizeof(ModDictKeyInfo))))
        goto error;
    if (!(key_table = PyMem_Malloc(dict_size * sizeof(digit))))
        goto error;
    if (!(mod_table = PyMem_Malloc(dict_size * sizeof(digit))))
        goto error;

    divmax = dict_size;
    dict_pos = rem_pos = 0;
    while (PyDict_Next(dict, &dict_pos, &key, &val)) {
        if (!PyLong_CheckExact(key))
            goto key_error;
        key_num = PyLong_AsLongLongAndOverflow(key, &overflow);
        if (overflow || (key_num < 0) || (key_num > digmax))
            goto key_error;
        divmax = Py_MAX(divmax, (digit) key_num);

        dict_keyidx[rem_pos].key = key_num;
        dict_keyidx[rem_pos].rem = 0;

        PyTuple_SET_ITEM(dict_keys, rem_pos, IncRef(key));
        PyTuple_SET_ITEM(dict_vals, rem_pos, IncRef(val));
        key_table[rem_pos] = key_num;
        key = val = NULL;
        rem_pos++;
    }
    if (!divmax)
        goto type_error;

    qsort(dict_keyidx, dict_size, sizeof(ModDictKeyInfo), ModDictKeyInfo_Compare);
    fdivisor = ModDict_find_divisor(divmax, dict_size, dict_keyidx);
    if (fdivisor < 0)
        goto error;
    if (fdivisor == 0)
        goto type_error;
    divisor = (digit) fdivisor;

    if (!(divisor_ = PyLong_FromUnsignedLong(divisor)))
        goto error;

    if (!(mod_keys = PyTuple_New(divisor)))
        goto error;
    for (key_pos = 0; key_pos < dict_size; key_pos++) {
        rem_pos = key_table[key_pos] % divisor;
        mod_table[key_pos] = rem_pos;
        PyTuple_SET_ITEM(mod_keys, rem_pos,
                         IncRef(PyTuple_GET_ITEM(dict_keys, key_pos)));
    }

    if (!(mod_vals = PyTuple_New(divisor)))
        goto error;
    if (!(remainder = PyMem_Malloc(divisor * sizeof(digit))))
        goto error;
    for (rem_pos = 0; rem_pos < divisor; rem_pos++)
        remainder[rem_pos] = divisor;
    for (key_pos = 0; key_pos < dict_size; key_pos++) {
        rem_pos = mod_table[key_pos];
        val = PyTuple_GET_ITEM(dict_vals, key_pos);
        PyTuple_SET_ITEM(mod_vals, rem_pos, IncRef(val));
        remainder[rem_pos] = key_pos;
    }
    val = NULL;

    if (!(rem_keys = PyMem_Malloc(divisor * sizeof(digit))))
        goto error;
    for (rem_pos = 0; rem_pos < divisor; rem_pos++)
        rem_keys[rem_pos] = ~0;
    for (key_pos = 0; key_pos < dict_size; key_pos++)
        rem_keys[mod_table[key_pos]] = PyLong_AsLongLong(PyTuple_GET_ITEM(dict_keys, key_pos));

    if (!(idict = PyDict_New()))
        goto error;
    if (divisor) {
        for (key_pos = 0; key_pos < dict_size; key_pos++) {
            key = PyTuple_GET_ITEM(dict_keys, key_pos);
            val = PyTuple_GET_ITEM(dict_vals, key_pos);
            if (PyDict_SetItem(idict, IncRef(key), IncRef(val)) < 0)
                goto error;
            ClearObject(&key);
            ClearObject(&val);
        }
    }

    self->dict = idict;
    self->divisor = divisor;
    self->divisor_ = divisor_;
    self->keys = mod_keys;
    self->values = mod_vals;
    self->remainder = remainder;
    self->rem_keys = rem_keys;

    res = 0;
    goto success;

key_error:
    PyErr_SetObject(PyExc_KeyError, key);
    goto error;
type_error:
    PyErr_BadArgument();
error:
    Py_XDECREF(idict);
    Py_XDECREF(divisor_);
    Py_XDECREF(mod_keys);
    Py_XDECREF(mod_vals);
    PyMem_Free(remainder);
    PyMem_Free(rem_keys);
success:
    Py_XDECREF(dict_keys);
    Py_XDECREF(dict_vals);
    Py_XDECREF(key);
    Py_XDECREF(val);
    PyMem_Free(dict_keyidx);
    PyMem_Free(key_table);
    PyMem_Free(mod_table);
    PyMem_Free(mod_gbuf);
    return res;
}

static int
ModDict_check_remainder(ModDictObject *self, PyObject *key)
{
    PyLongObject *lkey = (PyLongObject *) key;
    long long ikey;
    digit rem, nkey;
    int overflow;

    if (MODDICT_USE_LONGOBJECT) {
        if (Py_SIZE(key) < 1)
            return MODDICT_KEY_FAILED;
        if (Py_SIZE(key) > 2)
            return MODDICT_KEY_FAILED;
        nkey = lkey->ob_digit[0];
        if (Py_SIZE(key) == 2) {
            ikey = nkey | (lkey->ob_digit[1] << PyLong_SHIFT);
            if ((ikey >> 32))
                return MODDICT_KEY_FAILED;
            nkey = (digit) ikey;
        }
        rem = nkey % self->divisor;
        if (nkey != self->rem_keys[rem])
            return MODDICT_KEY_FAILED;
    }
    else {
        ikey = PyLong_AsLongLongAndOverflow(key, &overflow);
        if (overflow)
            return MODDICT_KEY_FAILED;
        rem = ikey % self->divisor;
        if (ikey != self->rem_keys[rem])
            return MODDICT_KEY_FAILED;
    }
    return (int) rem;
}

inline static int
ModDict_check_key(ModDictObject *self, PyObject *key)
{
    if (!PyLong_CheckExact(key))
        return MODDICT_KEY_ERROR;
    if (!self->divisor)
        return MODDICT_KEY_ERROR;
    return ModDict_check_remainder(self, key);
}

inline static PyObject *
ModDict_get_remainder_value(ModDictObject *self, int rem)
{
    return IncRef(PyTuple_GET_ITEM(self->values, rem));
}

inline static PyObject *
ModDict_get_value(ModDictObject *self, PyObject *key)
{
    int rem = ModDict_check_key(self, key);
    if (rem >= 0)
        return ModDict_get_remainder_value(self, rem);
    return SetKeyError(key);
}

static PyObject *
ModDict_from_dict(PyObject *dict)
{
    PyTypeObject *type = ModDictType;
    PyObject *newobj = NULL;
    PyObject *kwargs = NULL;
    PyObject *args = NULL;

    if (!dict)
        return NULL;
    if (!(args = PyTuple_New(1)))
        goto error;
    if (!(kwargs = PyDict_New()))
        goto error;
    PyTuple_SET_ITEM(args, 0, IncRef(dict));
    if (!(newobj = (type->tp_new)(type, args, kwargs)))
        goto error;
    if ((Py_TYPE(newobj)->tp_init)((PyObject *) newobj, args, kwargs) != 0)
        goto error;
    Py_XDECREF(args);
    Py_XDECREF(kwargs);
    return IncRef(newobj);

error:
    Py_XDECREF(args);
    Py_XDECREF(kwargs);
    Py_XDECREF(newobj);
    return NULL;
}

static PyObject *
ModDict_create_dict(PyObject *iterable, PyObject *defval)
{
    PyObject *dict = NULL;
    PyObject *it = NULL;
    PyObject *key = NULL;
    PyObject *val = NULL;
    Py_ssize_t pos;

    if (!(dict = PyDict_New()))
        goto error;
    if (!(it = PyObject_GetIter(iterable)))
        goto error;
    for (pos = 0; (key = PyIter_Next(it)); pos++) {
        if (defval)
            val = IncRef(defval);
        else if (!(val = PyLong_FromSsize_t(pos)))
            goto error;
        if (PyDict_SetItem(dict, key, val) < 0)
            goto error;
        ClearObject(&key);
        ClearObject(&val);
    }
    goto success;
error:
    ClearObject(&dict);
success:
    Py_XDECREF(key);
    Py_XDECREF(val);
    Py_XDECREF(it);
    return dict;
}

/* ******** */

static int
ModDict_init(ModDictObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { "iterable", "value", NULL, };

    PyObject *iterable = NULL;
    PyObject *value = NULL;

    PyObject *dict = NULL;
    int result = -1;

    UNUSED(kwargs);

    self->dict = NULL;
    self->divisor = 0;
    self->divisor_ = NewNone();
    self->keys = NewNone();
    self->values = NewNone();
    self->remainder = NULL;
    self->rem_keys = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O",
                                     kwlist, &iterable, &value))
        return -1;
    if (PyDict_Check(iterable))
        dict = IncRef(iterable);
    else if (!(dict = ModDict_create_dict(iterable, value)))
        goto error;
    result = ModDict_create_table(self, dict);
error:
    Py_XDECREF(dict);
    return result;
}

static void
ModDict_finalize(ModDictObject *self)
{
    Py_XDECREF(self->dict);
    Py_XDECREF(self->divisor_);
    Py_XDECREF(self->keys);
    Py_XDECREF(self->values);
    PyMem_Free(self->remainder);
    PyMem_Free(self->rem_keys);
}

/* ******** */

static PyObject *
ModDict_divisor(ModDictObject *self)
{
    return IncRef(self->divisor_);
}

static PyObject *
ModDict_create_mkvtable(PyObject *table, PyObject *defval)
{
    PyObject *rval = NULL;
    PyObject *obj = NULL;
    Py_ssize_t size, pos;

    size = PyTuple_Size(table);
    if (!(rval = PyTuple_New(size)))
        return NULL;
    for (pos = 0; pos < size; pos++) {
        obj = PyTuple_GET_ITEM(table, pos);
        if (IsNull(obj))
            obj = defval;
        PyTuple_SET_ITEM(rval, pos, IncRef(obj));
    }
    return rval;
}

static PyObject *
ModDict_modkeys(ModDictObject *self, PyObject *args)
{
    PyObject *defval = Py_None;

    if (!PyArg_ParseTuple(args, "|O", &defval))
        return NULL;
    if (!self->divisor)
        return PyTuple_New(0);
    return ModDict_create_mkvtable(self->keys, defval);
}

static PyObject *
ModDict_mkvalues(ModDictObject *self, PyObject *args)
{
    PyObject *defval = Py_None;

    if (!PyArg_ParseTuple(args, "|O", &defval))
        return NULL;
    if (!self->divisor)
        return PyTuple_New(0);
    return ModDict_create_mkvtable(self->values, defval);
}

static PyObject *
ModDict_remainder_index(ModDictObject *self, PyObject *args)
{
    PyObject *defval = Py_None;
    PyObject *rval = NULL;
    PyObject *remainder;
    Py_ssize_t rem_pos;
    digit index;

    if (!PyArg_ParseTuple(args, "|O", &defval))
        return NULL;
    if (!self->divisor)
        return PyTuple_New(0);
    if (!(rval = PyTuple_New(self->divisor)))
        goto error;
    for (rem_pos = 0; rem_pos < self->divisor; rem_pos++) {
        index = self->remainder[rem_pos];
        if (index >= self->divisor)
            remainder = IncRef(defval);
        else if (!(remainder = PyLong_FromUnsignedLong(index)))
            goto error;
        PyTuple_SET_ITEM(rval, rem_pos, remainder);
    }
    return rval;

error:
    Py_XDECREF(rval);
    return NULL;
}

static PyObject *
ModDict_forindex(PyObject *klass, PyObject *iterable)
{
    PyObject *obj = NULL;
    PyObject *dict = NULL;

    UNUSED(klass);
    if ((dict = ModDict_create_dict(iterable, NULL)))
        obj = ModDict_from_dict(dict);
    Py_XDECREF(dict);
    return obj;
}

/* ******** */

static PyObject *
ModDict___repr__(ModDictObject *self)
{
    return Py_TYPE(self->dict)->tp_repr(self->dict);
}

static PyObject *
ModDict___iter__(ModDictObject *self)
{
    return Py_TYPE(self->dict)->tp_iter(self->dict);
}

static PyObject *
ModDict___contains__(ModDictObject *self, PyObject *key)
{
    return NewBool(ModDict_check_key(self, key) >= 0);
}

static PyObject *
ModDict___getitem__(ModDictObject *self, PyObject *key)
{
    return ModDict_get_value(self, key);
}

static int
ModDict_sequence_contains(ModDictObject *self, PyObject *key)
{   /* obj[key] */
    return (ModDict_check_key(self, key) >= 0);
}

static Py_ssize_t
ModDict_length(ModDictObject *self)
{
    return Py_SIZE(self->dict);
}

static PyObject *
ModDict_subscript(ModDictObject *self, PyObject *key)
{
    return ModDict_get_value(self, key);
}

static int
ModDict_ass_subscript(ModDictObject *self, PyObject *key, PyObject *item)
{
    UNUSED(self);
    UNUSED(key);
    UNUSED(item);
    PyErr_SetString(PyExc_TypeError, "ModDict does not support item assignment");
    return -1;
}

static PyObject *
ModDict_get(ModDictObject *self, PyObject *args)
{
    PyObject *key = NULL;
    PyObject *defval = Py_None;
    int rem;

    if (!PyArg_ParseTuple(args, "O|O", &key, &defval))
        return NULL;
    if ((rem = ModDict_check_key(self, key)) >= 0)
        return ModDict_get_remainder_value(self, rem);
    return IncRef(defval);
}

static PyObject *
ModDict_keys(ModDictObject *self)
{
    return PyDict_Keys(self->dict);
}

static PyObject *
ModDict_values(ModDictObject *self)
{
    return PyDict_Values(self->dict);
}

static PyObject *
ModDict_items(ModDictObject *self)
{
    return PyDict_Items(self->dict);
}

static PyObject *
ModDict_dict(ModDictObject *self)
{
    return PyDict_Copy(self->dict);
}

/*
 * Type: ModDict
 */

static PySequenceMethods ModDict_as_sequence = {
    .sq_contains = (objobjproc) ModDict_sequence_contains,
};

static PyMappingMethods ModDict_as_mapping = {
    (lenfunc) ModDict_length,
    (binaryfunc) ModDict_subscript,
    (objobjargproc) ModDict_ass_subscript,
};

static PyMethodDef ModDict_methods[] = {
    {"__contains__", (PyCFunction) ModDict___contains__, METH_O, NULL},
    {"__getitem__", (PyCFunction) ModDict___getitem__, METH_O, NULL},

    {"divisor", (PyCFunction) ModDict_divisor, METH_NOARGS, NULL},
    {"modkeys", (PyCFunction) ModDict_modkeys, METH_VARARGS, NULL},
    {"mkvalues", (PyCFunction) ModDict_mkvalues, METH_VARARGS, NULL},
    {"remainder_index", (PyCFunction) ModDict_remainder_index, METH_VARARGS, NULL},
    {"forindex", (PyCFunction) ModDict_forindex, METH_O | METH_CLASS, NULL},

    {"get", (PyCFunction) ModDict_get, METH_VARARGS, NULL},
    {"keys", (PyCFunction) ModDict_keys, METH_NOARGS, NULL},
    {"values", (PyCFunction) ModDict_values, METH_NOARGS, NULL},
    {"items", (PyCFunction) ModDict_items, METH_NOARGS, NULL},

    {"dict", (PyCFunction) ModDict_dict, METH_NOARGS, NULL},

#if PY_VERSION_HEX >= 0x03090000
    {"__class_getitem__", (PyCFunction) Py_GenericAlias, METH_O | METH_CLASS, NULL},
#endif

    {NULL, NULL, 0, NULL}, /* end */
};


#undef ModDictType
static PyTypeObject ModDictType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "ModDict.ModDict",
    .tp_doc = "ModDict object",
    .tp_basicsize = sizeof(ModDictObject),
    .tp_itemsize = 0,
    .tp_flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
                 Py_TPFLAGS_DICT_SUBCLASS | Py_TPFLAGS_MAPPING),

    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) ModDict_init,
    .tp_finalize = (destructor) ModDict_finalize,

    .tp_repr = (reprfunc) ModDict___repr__,
    .tp_str = (reprfunc) ModDict___repr__,

    .tp_as_sequence = &ModDict_as_sequence,
    .tp_as_mapping = &ModDict_as_mapping,
    .tp_methods = ModDict_methods,

    .tp_iter = (getiterfunc) ModDict___iter__,
};

inline static PyTypeObject *
get_moddict_type()
{
    return &ModDictType;
}


/*
 * Module: ModDict
 */

typedef struct ModDictState {
    PyObject *error;
} ModDictState;

static PyModuleDef ModDict_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "ModDict",
    .m_doc = "extension module for read-only dictionary with key as 32-bit unsigned integer.",
    .m_size = -1,
};

PyMODINIT_FUNC
PyInit_ModDict(void)
{
    PyObject *module;

    if (PyType_Ready(&ModDictType) < 0)
        return NULL;
    if (!(module = PyModule_Create(&ModDict_def)))
        return NULL;

    Py_INCREF(&ModDictType);
    if (PyModule_AddObject(module, "ModDict", (PyObject *) &ModDictType) < 0) {
        Py_DECREF(module);
        Py_DECREF(&ModDictType);
        return NULL;
    }
    return module;
}

/*
 * Local Variables:
 * c-file-style: "PEP7"
 * End:
 */
