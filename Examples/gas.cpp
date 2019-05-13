#include <stdint.h>
#include <stdlib.h>

uint64_t sub(uint64_t b) {
	return b - 10;
}

uint64_t add(uint64_t a) {
	a += 10;
	uint64_t t = 10;
	for(int i=0;i < 100;i ++) {
		for (int j=0; j< a; j ++) {
			 t += i + j;
			 if (t > 100) {
			 	for(int i=0;i<2;i++) {
					sub(t);
				}
			 } else {
			 	sub(t);
			 }
		}
	}
	return sub(t);
}

int main() {
	uint64_t a = add(10);
	a += 10;
	return a;
}
