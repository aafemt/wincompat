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

void RunPathTests();
void RunSyncTests();

int main()
{
	setlocale(LC_ALL, "");

	RunFindTests();
	RunNlsTests();
	RunPathTests();
	RunSyncTests();

	if (failed)
	{
		printf("Some test failed.\n");
		return 1;
	}
	printf("All ok.\n");
	return 0;
}
