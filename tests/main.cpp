/*******************************************************
*
*   Main program that runs everything else
*
********************************************************/

#include <stdio.h>

#include "globals.h"
#include "find.h"

int main()
{
	RunFindTests();

	if (failed)
	{
		printf("Some test failed.\n");
		return 1;
	}
	printf("All ok.\n");
	return 0;
}
