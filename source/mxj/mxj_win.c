#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <jni.h>
#include "mxj_win.h"

jboolean debug=true;

//from launcher/java.c

#ifndef FULL_VERSION
#define FULL_VERSION JDK_MAJOR_VERSION "." JDK_MINOR_VERSION
#endif

#ifdef WIN32
#define PATHSEP "\\"
#else
#define PATHSEP "/"
#endif

//from launcher/java.c

#ifdef DEBUG
#define JVM_DLL "jvm_g.dll"
#define JAVA_DLL "java_g.dll"
#else
#define JVM_DLL "jvm.dll"
#define JAVA_DLL "java.dll"
#endif

/*
 * Prototypes.
 */
static jboolean GetPublicJavaPaths(char *javaHomePath, jint javaHomePathSize, char *runtimeLibraryPath, jint runtimeLibraryPathSize);
static jboolean GetJavaPaths(char *javaHomePath, jint javaHomePathSize, char *runtimeLibraryPath, jint runtimeLibraryPathSize);

const char *
GetArch()
{
#ifdef _WIN64
	return "ia64";
#else
	return "i386";
#endif
}

void AddJavaBinFolderToPath(const char *javahome)
{
	CHAR* next_path = NULL;
	CHAR* current_path = NULL;
	CHAR binpath[MAXPATHLEN];

	long len;

	strcpy(binpath, javahome);
	if (binpath[strlen(binpath)-1] != '\\') {
		strcat(binpath, "\\");
	}
	strcat(binpath, "bin");

	// Get current path
	len = GetEnvironmentVariable("path", current_path, 0);
	const long cplen = len * sizeof(CHAR) + 1;
	current_path = (CHAR*)sysmem_newptr(cplen);
	GetEnvironmentVariable("path", current_path, cplen);

	len = cplen + (long)strlen(binpath) + 1; // 1 for ;
	next_path = (CHAR*)sysmem_newptr(len * sizeof(CHAR));
	if (next_path) {
		// JRE path first, it will be used first when searching path
		strcpy(next_path, binpath);
		strcat(next_path, ";");
		strcat(next_path, current_path);

		SetEnvironmentVariable("path", next_path);
		sysmem_freeptr(next_path);
		sysmem_freeptr(current_path);
	}
}

/*
 *
 */
long
CreateExecutionEnvironment(
						   char jrepath[],
						   jint so_jrepath,
						   char jvmpath[],
						   jint so_jvmpath) 
{
	struct stat s;

	/* Find out where the JRE is that we will be using. */
	if (!GetJavaPaths(jrepath, so_jrepath, jvmpath, so_jvmpath)) {
		ReportErrorMessage("could not find Java 2 Runtime Environment.",JNI_TRUE);
		return 2;
	}

#if 0
	/* If we got here, jvmpath has been correctly initialized. */
	// HACK...the above code is crashing when reading the config file.
	// currently assuming we will load the following file. revisit. -jkc
	sprintf(jvmpath, "%s\\bin\\client\\"JVM_DLL, jrepath);
	if (stat(jvmpath, &s) != 0) {
		// rbs: on my install of the JVM for x64 the jvm.dll was in this path
		// not sure what the correct way is to get this path, but this will do for
		// now
		sprintf(jvmpath, "%s\\bin\\server\\"JVM_DLL, jrepath);
		if (stat(jvmpath, &s) != 0) {
			ReportErrorMessage("could not find JRE client or server "JVM_DLL, JNI_TRUE);
			return 2;
		}
		*_jvmtype = _strdup("server");
	}
	else { *_jvmtype = _strdup("client"); }
#endif

	if (debug) {
		post("Found "JVM_DLL" here: %s\n", jvmpath);
	}
	return 0;
}

/*
 * Find path to JRE based on .exe's location (embeded into application)
 * or registry settings (installed in the computer programs)
 */
jboolean
GetJavaPaths(char *javaHomePath, jint javaHomePathSize, char *runtimeLibraryPath, jint runtimeLibraryPathSize)
{
	char javadll[MAXPATHLEN];
	struct stat s;

	if (GetApplicationHome(javaHomePath, javaHomePathSize)) {
		if (debug) {
			post("Searching for " JAVA_DLL " in application home directory: %s\n", javaHomePath);
		}
		/* Is JRE co-located with the application? */
		sprintf(javadll, "%s\\bin\\" JAVA_DLL, javaHomePath);
		if (stat(javadll, &s) == 0) {
			strncpy_zero(runtimeLibraryPath, javadll, runtimeLibraryPathSize);
			if (debug) {
				post("Found %s\n", javadll);
			}
			goto found;
		}
		if (debug) {
			post("Didn't find %s\n", javadll);
		}

		/* Does this app ship a private JRE in <apphome>\jre directory? */
		sprintf(javadll, "%s\\jre\\bin\\" JAVA_DLL, javaHomePath);
		if (stat(javadll, &s) == 0) {
			strcat(javaHomePath, "\\jre");
			strncpy_zero(runtimeLibraryPath, javadll, runtimeLibraryPathSize);
			if (debug) {
				post("Found %s\n", javadll);
			}
			goto found;
		}
		if (debug) {
			post("Didn't find %s\n", javadll);
		}
	}

	/* Look for a public JRE on this machine. */
	if (debug) {
		post("Looking for a public JRE on this machine...\n");
	}
	if (GetPublicJavaPaths(javaHomePath, javaHomePathSize, runtimeLibraryPath, runtimeLibraryPathSize)) {
		goto found;
	}

	error("Error: could not find " JAVA_DLL "\n");
	return JNI_FALSE;

found:
	if (debug)
		post("JRE path is %s\n", javaHomePath);
	return JNI_TRUE;
}

/*
 * Load a jvm from "jvmpath" and intialize the invocation functions.
 */
jboolean
LoadJavaVM(const char *jvmpath, InvocationFunctions *ifn)
{
	HINSTANCE handle;

	if (debug) {
		post("JVM path is %s\n", jvmpath);
	}

	/* Load the Java VM DLL */
	if ((handle = LoadLibrary(jvmpath)) == 0) {
		ReportErrorMessage2("Error loading: %s", (char *)jvmpath, JNI_TRUE);
		return JNI_FALSE;
	}

	/* Now get the function addresses */
	ifn->CreateJavaVM = (void *)GetProcAddress(handle, "JNI_CreateJavaVM");
	ifn->GetDefaultJavaVMInitArgs =		(void *)GetProcAddress(handle, "JNI_GetDefaultJavaVMInitArgs");
	if (ifn->CreateJavaVM == 0 || ifn->GetDefaultJavaVMInitArgs == 0) {
		ReportErrorMessage2("Error: can't find JNI interfaces in: %s", (char *)jvmpath, JNI_TRUE);
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

/*
 * Get the path to the file that has the usage message for -X options.
 */
void
GetXUsagePath(char *buf, jint bufsize)
{
	GetModuleFileName(GetModuleHandle(JVM_DLL), buf, bufsize);
	*(strrchr(buf, '\\')) = '\0';
	strcat(buf, "\\Xusage.txt");
}

/*
 * If app is "c:\foo\bin\javac", then put "c:\foo" into buf.

 bbn: above is a mistake.  we are not using this to find the home of javac, we are using it to find the home of the Max app!!
 */
jboolean
GetApplicationHome(char *buf, jint bufsize)
{
	GetModuleFileName(0, buf, bufsize);
	*strrchr(buf, '\\') = '\0'; /* remove .exe file name */
	return 1;
}


/*
 * Helpers to look in the registry for a public JRE.
 */

static jboolean
GetStringFromRegistry(HKEY key, const char *name, char *buf, jint bufsize)
{
	DWORD type, size;

	if (RegQueryValueEx(key, name, 0, &type, 0, &size) == 0
		&& type == REG_SZ
		&& (size < (unsigned int)bufsize)) {
		if (RegQueryValueEx(key, name, 0, 0, buf, &size) == 0) {
			return JNI_TRUE;
		}
	}
	return JNI_FALSE;
}

static const char* JRE_Keys[] = {
	"SOFTWARE\\JavaSoft\\JDK", // look for the new one first
	"SOFTWARE\\JavaSoft\\Java Development Kit",
	"SOFTWARE\\JavaSoft\\JRE", // look for the new one first
	"SOFTWARE\\JavaSoft\\Java Runtime Environment",
	NULL
};

static jboolean
GetPublicJavaPaths(char *javaHomePath, jint javaHomePathSize, char *runtimeLibraryPath, jint runtimeLibraryPathSize)
{
	HKEY key, subkey;
	char version[MAXPATHLEN];
	char rtlib[MAXPATHLEN];
	const char **jrekey = JRE_Keys;

	if (debug) {
		post("Looking for a public JRE in the registry... starting with %s\n", *jrekey);
	}

	while (*jrekey != NULL) {
		if (debug) {
			post("Looking for registry key '%s'...\n", *jrekey);
		}
		/* Find the current version of the JRE */
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, *jrekey, 0, KEY_READ, &key) != 0) {
			if (debug) {
				post("Error opening registry key '%s'\n", *jrekey);
			}
			jrekey++;
			continue;
		}
		if (debug) {
			post("Found registry key '%s'\n", *jrekey);
		}

		if (!GetStringFromRegistry(key, "CurrentVersion", version, sizeof(version))) {
			if (debug) {
				post("Failed reading value of registry key:\n\t%s\\CurrentVersion\n", *jrekey);
			}
			RegCloseKey(key);
			jrekey++;
			continue;
		}
		if (debug) {
			post("CurrentVersion of registry key '%s' is '%s'\n", *jrekey, version);
		}

		/* Find directory where the current version is installed. */
		if (RegOpenKeyEx(key, version, 0, KEY_READ, &subkey) != 0) {
			if (debug) {
				post("Error opening registry key '%s\\%s'\n", *jrekey, version);
			}
			RegCloseKey(key);
			jrekey++;
			continue;
		}
		if (debug) {
			post("Found registry key '%s\\%s'\n", *jrekey, version);
		}

		if (!GetStringFromRegistry(subkey, "JavaHome", javaHomePath, javaHomePathSize)) {
			if (debug) {
				post("Failed reading value of registry key:\n\t%s\\%s\\JavaHome\n", *jrekey, version);
			}
			RegCloseKey(key);
			RegCloseKey(subkey);
			jrekey++;
			continue;
		}
		if (debug) {
			post("JavaHome of registry key '%s\\%s' is '%s'\n", *jrekey, version, javaHomePath);
		}

		// ensure that this path also contains a runtime dll, otherwise...
		// TODO: should we search the javaHome for a runtime lib in this case if none is found?
		if (!GetStringFromRegistry(subkey, "RuntimeLib", runtimeLibraryPath, runtimeLibraryPathSize)) {
			if (debug) {
				post("Failed reading value of registry key:\n\t%s\\%s\\RuntimeLib\n", *jrekey, version);
			}

			// Check if we can find the runtime library in the java home path, as modern versions of 
			// the JDK/JRE don't always have the RuntimeLib registry key. If we can find it there, then we can use this JRE/JDK.
			char javadll[MAXPATHLEN];
			sprintf(javadll, "%s\\bin\\server\\" JVM_DLL, javaHomePath);
			struct stat s;
			bool found = false;
			if (stat(javadll, &s) == 0) {
				strncpy_zero(runtimeLibraryPath, javadll, runtimeLibraryPathSize);
				found = true;
			}
			if (!found) {
				RegCloseKey(key);
				RegCloseKey(subkey);
				jrekey++;
				continue;
			}
		}
		if (debug) {
			post("RuntimeLib found is version %s: '%s'\n", version, runtimeLibraryPath);
		}

		// MicroVersion is not present in all versions of the JRE, but if it is present, print it out for debugging purposes.
		/*
		if (debug) {
			char micro[MAXPATHLEN];
			if (!GetStringFromRegistry(subkey, "MicroVersion", micro, sizeof(micro))) {
				ReportErrorMessage("Warning: Can't read MicroVersion\n", true);
				micro[0] = '\0';
			}
			post("Version major.minor.micro = %s.%s\n", version, micro);
		}
		*/

		RegCloseKey(key);
		RegCloseKey(subkey);
		return JNI_TRUE;
	}
	return JNI_FALSE;
}

/*
 * Support for doing cheap, accurate interval timing.
 */
static jboolean counterAvailable = JNI_FALSE;
static jboolean counterInitialized = JNI_FALSE;
static LARGE_INTEGER counterFrequency;

jlong CounterGet()
{
	LARGE_INTEGER count;

	if (!counterInitialized) {
		counterAvailable = QueryPerformanceFrequency(&counterFrequency);
		counterInitialized = JNI_TRUE;
	}
	if (!counterAvailable) {
		return 0;
	}
	QueryPerformanceCounter(&count);
	return (jlong)(count.QuadPart);
}

jlong Counter2Micros(jlong counts)
{
	if (!counterAvailable || !counterInitialized) {
		return 0;
	}
	return (counts * 1000 * 1000)/counterFrequency.QuadPart;
}

void ReportErrorMessage(char * message, jboolean always) {
	if (always) {
		error("mxj: %s\n", message);
	}
}

void ReportErrorMessage2(char * format, char * string, jboolean always) {
	/*
	 * The format argument must be a printf format string with one %s
	 * argument, which is passed the string argument.
	 */
	if (always) {
		error(format, string);
	}
}

/*
 * Return JNI_TRUE for an option string that has no effect but should
 * _not_ be passed on to the vm; return JNI_FALSE otherwise. On
 * windows, there are no options that should be screened in this
 * manner.
 */
jboolean RemovableMachineDependentOption(char * option) {
	return JNI_FALSE;
}

void PrintMachineDependentOptions() {
	return;
}

// our stuff

char g_JavaHomePath[MAXPATHLEN], g_RuntimeLibPath[MAXPATHLEN];
InvocationFunctions g_InvocationFunctions;

const char *getGlobal_JavaHomePath() { return g_JavaHomePath; }
const char *getGlobal_RuntimeLibPath() { return g_RuntimeLibPath; }

long mxj_platform_init()
{
	g_JavaHomePath[0] = g_RuntimeLibPath[0] = 0;
	CreateExecutionEnvironment(g_JavaHomePath, sizeof(g_JavaHomePath), g_RuntimeLibPath, sizeof(g_RuntimeLibPath));
	g_InvocationFunctions.CreateJavaVM = 0;
	g_InvocationFunctions.GetDefaultJavaVMInitArgs = 0;

	AddJavaBinFolderToPath(g_JavaHomePath);

	if (!LoadJavaVM(g_RuntimeLibPath, &g_InvocationFunctions)) {
		return -1;
	}
	return 0;
}
