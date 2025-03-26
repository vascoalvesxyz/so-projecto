#include <stdio.h>
#include <stdlib.h>
//É preciso já criar uma transaction?
int main(int argc, char *argv[]) {
    int i, n_procs;

	if(argc!=3) {
		printf("Wrong number of parameters\n");
		exit(0);
		}
	if(atoi(argv[1])>=1 && atoi(argv[1])<=3 && atoi(argv[2])<=3000 && atoi(argv[2]) >=200){
		printf("The parameters are correct\n");
	}
   
	
	return 0;
}
