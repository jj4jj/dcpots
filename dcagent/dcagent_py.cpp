#include "dcagent.h"
#include "python2.7/Python.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


PyMODINIT_FUNC
	initdagent(void)
{
		dagent_export_python(true);
}


#ifdef __cplusplus
}
#endif // __cplusplus
