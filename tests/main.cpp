/*******************************************************
*
*   Main program that runs everything else
*
********************************************************/

#include <locale.h>
#include <stdio.h>

#include "globals.h"
#include "find.h"
#include "nls.h"

void RunEnvTests();
void RunPathTests();
void RunSyncTests();
void RunDllTests();

int main()
{
	setlocale(LC_ALL, "");

	RunEnvTests();
	RunFindTests();
	RunNlsTests();
	RunPathTests();
	RunSyncTests();
	RunDllTests();

	if (failed)
	{
		printf("Some test failed.\n");
		return 1;
	}
	printf("All ok.\n");
	return 0;
}
