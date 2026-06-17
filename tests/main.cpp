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
void RunStringTests();

int main()
{
	if (setlocale(LC_ALL, "") == nullptr)
	{
		failure("setlocale in main returned null\n");
		return 1;
	}

	RunStringTests();
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
