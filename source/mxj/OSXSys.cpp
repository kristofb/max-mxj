#include "OSXSys.h"
#include <errno.h>
#include <sys/sysctl.h>
#include <string.h>
#include <stdlib.h>

static int majorOSVersion = -1;

int findMajorOSVersion()
{
    if (majorOSVersion == -1)
    {
#ifdef WIN_VERSION
        majorOSVersion = 0;
#else
        char osrelease[256];
        size_t size = sizeof(osrelease);
        bzero(osrelease, size);
        majorOSVersion = 0;
        int ret = sysctlbyname("kern.osrelease", osrelease, &size, NULL, 0);

        // non zero is an error
        if (ret != 0)
        {
        }
        else
        {
            majorOSVersion = (int)strtol(osrelease, NULL, 10); // No split needed, it will stop at the first dot and give us the major version
        }
#endif
    }

    return majorOSVersion;
}
