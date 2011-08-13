#include <errno.h>
#include <string.h>

//\file depends: strerror, [sreturnf]
/**
 ** \returns a string with a human-readable error name
 ** \param error - errno to be "stringized"
 **/
char* e_name(int error)
{
    char *s;
    
    switch (error) {
        case -1:
            s = sreturnf("<any>");
        break;
        case EINVAL:
            s = sreturnf("EINVAL");
        break;
        case ERANGE:
            s = sreturnf("ERANGE");
        break;
        case ENOMEM:
            s = sreturnf("ENOMEM");
        break;
        case E2BIG:
            s = sreturnf("E2BIG");
        break;
        case ENAMETOOLONG:
            s = sreturnf("ENAMETOOLONG");
        break;
        case EINTR:
            s = sreturnf("EINTR");
        break;
        default:
            s = sreturnf("%i(%s)", error, strerror(error));
        break;
    }
    return s;
}
