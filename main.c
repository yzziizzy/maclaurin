#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>

#include <pthread.h>

// #include "ds.h"




#define KILOBYTE (1024ul)
#define MEGABYTE (KILOBYTE * 1024)
#define GIGABYTE (MEGABYTE * 1024)
#define TERABYTE (GIGABYTE * 1024)
#define PETABYTE (TERABYTE * 1024)

#define KILOBYTES(x) ((x) * 1024ul)
#define MEGABYTES(x) ((x) * KILOBYTE * 1024)
#define GIGABYTES(x) ((x) * MEGABYTE * 1024)
#define TERABYTES(x) ((x) * GIGABYTE * 1024)
#define PETABYTES(x) ((x) * TERABYTE * 1024)


// uint8_t quotients[257][257];
// uint8_t remainders[257][257];

size_t g_mem_usage = 0;

static void pretty_print_mem(size_t sz);

typedef struct {
	uint32_t* data;
	uint32_t length;
	uint32_t divisor;
	uint32_t remainder;
	
	pthread_mutex_t lock;
	
	
	char finished;
} Fraction;


typedef struct {
	struct {
		uint64_t length;
		Fraction* fr;
	}* fractions;
	
	size_t alloc_sz, length;
	
	pthread_mutex_t lock;
	
} FStore;


void add_fraction(Fraction* into, Fraction* addend);
void free_fraction(Fraction* fr);


void init_fstore(FStore* fs) {
	fs->alloc_sz = 1024*64;
	fs->length = 0;
	fs->fractions = calloc(1, sizeof(*fs->fractions) * fs->alloc_sz); 

	pthread_mutex_init(&fs->lock, NULL);
}


void fstore_push(FStore* fs, Fraction* fr) {

	
	int32_t flen = fr->length;
	size_t lowest_i = 0;
	size_t lowest_len = 0;
	
	for(size_t i = 0; i < fs->length; i++) {
// 		if(flen > fs->fractions[i].length) continue;
		
		int32_t l = fs->fractions[i].length; 
		if(flen <= l && l % flen == 0) {
			
			
			if(lowest_len == 0 || l < lowest_len) {
				lowest_i = i;
				lowest_len = l;
			}
			continue;
		}
		else if(flen > l && flen % l == 0) {
// 			printf("-%d goes into %d \n", l, flen);
			/*
			add_fraction(fr, fs->fractions[i].fr);
			
			free_fraction(fs->fractions[i].fr);
			fs->fractions[i].fr = fr;
			fs->fractions[i].length = flen;
			*/
			
			if(lowest_len == 0 || l < lowest_len) {
				lowest_i = i;
				lowest_len = l;
			}
			
			
			continue;
		}
	}
	
	if(lowest_len) {
		
		
// 		pthread_mutex_lock(&fs->lock);
//  		pthread_mutex_lock(&fs->fractions[lowest_i].lock);
// 		pthread_mutex_unlock(&fs->lock);
		
		
		printf("%d goes into %ld at [%ld]\n", flen, lowest_len, lowest_i);
		if(flen > lowest_len) {
			add_fraction(fr, fs->fractions[lowest_i].fr);
			
			free_fraction(fs->fractions[lowest_i].fr);
			
			pthread_mutex_lock(&fs->lock);
			fs->fractions[lowest_i].fr = fr;
			fs->fractions[lowest_i].length = flen;
			pthread_mutex_unlock(&fs->lock);

		}
		else {
			add_fraction(fs->fractions[lowest_i].fr, fr);
			free_fraction(fr);
		}
		
// 		pthread_mutex_unlock(&fs->fractions[lowest_i].lock);
// 		pthread_mutex_unlock(&fs->lock);
	
		return;
	}

	fr->data = realloc(fr->data, flen * sizeof(fr->data));
	
	// else append
	pthread_mutex_lock(&fs->lock);
	
	if(fs->length >= fs->alloc_sz) {
		fs->alloc_sz *= 2;
		fs->fractions = realloc(fs->fractions, sizeof(*fs->fractions) * fs->alloc_sz); 
	} 
	
	fs->fractions[fs->length].length = flen;
	fs->fractions[fs->length].fr = fr;
// 	pthread_mutex_init(&fs->fractions[fs->length].lock, NULL);
	fs->length++;
	
	g_mem_usage += flen * 4;
	
	if(g_mem_usage > GIGABYTES(2)) {
		printf("limit exceeded\n");
		exit(0);
	} 
	
	pthread_mutex_unlock(&fs->lock);

	printf("fstore length: %ld\n", fs->length);
}



void free_fraction(Fraction* fr) {
	pthread_mutex_lock(&fr->lock);
	if(fr) {
		free(fr->data);
	}
	free(fr);
}


void add_fraction(Fraction* into, Fraction* addend) {
// 	printf("adding %d\n", addend->length);
	
	pthread_mutex_lock(&into->lock);
	
	uint32_t a, i;
	uint32_t* out = into->data, *in = addend->data;
	uint32_t len = into->length;
	uint32_t alen = addend->length;
	uint32_t carry = 0;
	
	for(a = 0, i = 0; i < len; a++, i++) {
		if(a >= alen) a = 0;
		
		uint64_t sum = (uint64_t)out[i] + (uint64_t)in[a] + carry;
		out[i] = (uint32_t)sum;
		
		carry = sum >> 32;
	}
	
	// wrap the carry around to the front
	i = 0;
	a = 0;
	while(carry) {
		
		uint64_t sum = (uint64_t)out[i] + (uint64_t)in[a] + carry;
		out[i] = (uint32_t)sum;
		
		carry = sum >> 32;
		
		i++;
		a++;
		
		if(a >= alen) a = 0;
		if(i >= len) i = 0;
	}
	
	pthread_mutex_unlock(&into->lock);
	
}



Fraction* fill_fraction(Fraction* fr, uint32_t sz) {
	uint32_t i;
	
	Fraction* of = malloc(sizeof(of));
	of->length = sz;
	of->data = malloc(sz * sizeof(*of->data));
	of->divisor = 0;
	
	uint32_t* out = of->data, *in = fr->data;
	uint32_t alen = fr->length;
	uint32_t len = sz;
	
	for(i = 0; i < len; i++) {
		out[i] = in[i % alen];
	}
	
	return of;
}



Fraction* new_fraction32(uint32_t divisor, uint32_t max_len) {
	
	int finished = 0;
	
	int32_t r;
	int32_t b = divisor;
	int64_t i = 0;
	int32_t lz = 0;
	
	int32_t* o = malloc(max_len * sizeof(*o));
	
	Fraction* fr = malloc(sizeof(*fr));
	fr->data = o;
	fr->divisor = b;
	pthread_mutex_init(&fr->lock, NULL);
	
	int64_t t = 1ul << 32;
	
	int32_t first_q = t / b;
	int32_t first_r = t % b;
	if(first_q == 0) lz++;
	
// 		;
	o[0] = first_q;
	
	t = (int64_t)first_r << 32;
	
	for(i = 1; i < max_len; i++) {
		int32_t q;
		
		q = t / b;
		r = t % b;
		if(q == 0) lz++;
		
		o[i] = q;
// 			printf(" %d,%d, %ld\n", q, r, t);
		
		if(first_q == q && first_r == r) {
			finished = 1;
			break;
		}
		
		t = (int64_t)r << 32;
	}
	
	if(lz > 0) {
		printf("leading zeros: %d (%d)\n", lz, b);
	}
	
	fr->finished = finished;
	if(!finished) {
		// store remainder for later processing
		fr->remainder = r;
	}
	
	fr->length = i;

	return fr;
}

typedef struct {
	pthread_t thread;
	_Atomic int should_die;
	FStore* fs;
} ThreadInfo;


_Atomic uint64_t next_divisor;


int64_t MAX = 1024*16*16;

void* work_thread_fn(ThreadInfo* ti) {
	
	while(1) {
		uint64_t b = atomic_fetch_add(&next_divisor, 2);
		
		Fraction* fr = new_fraction32(b, MAX);
		
		if(b % 1001 == 0) {
			pretty_print_mem(g_mem_usage);
			printf("\n");
		}
		
		if(fr->length == MAX) {
			printf("%ld: %d\n", b, fr->length);
			break;
		}
		else {
	// 			int32_t i = fr->length;
			fstore_push(ti->fs, fr);
		}
	}
	
	return NULL;
}



int main(int argc, char* argv[]) {
	
	int num_threads = 11;
	
	ThreadInfo threads[num_threads];
	
	atomic_init(&next_divisor, 3);
	
	
	FStore* fs = malloc(sizeof(*fs));
	init_fstore(fs);
// 	Fraction* sixty = NULL;
	
// 	VEC(Fraction*) fractions;
// 	VEC_INIT(&fractions);
	/*
	for(int i = 1; i < 2; i++) {
		for(int j = 3; j <= 256; j+=2) {
			int q = (i << 8) / j;
			int r = (i << 8) % j;
			
			quotients[i][j] = q;
			remainders[i][j] = r;
			
// 			printf(" %d / %d = [%d , %d] \n", i, j, q, r);
			
		}
	}
	*/
	

	
	
	for(int i = 0; i < num_threads; i++) {
		ThreadInfo* t = &threads[i];
		
		t->fs = fs;
		atomic_init(&t->should_die, 0);
		
		pthread_create(&t->thread, NULL, (void*(*)(void*))work_thread_fn, t);
	}
	
	
	/*
	for(int64_t b = 3; b < 256*64*128; b += 2) {
		
		Fraction* fr = new_fraction32(b, MAX);
		
		if(b % 1001 == 0) {
			pretty_print_mem(g_mem_usage);
			printf("\n");
		}
		
		
		if(fr->length == MAX) {
			printf("%ld: %d\n", b, fr->length);
			break;
		}
		else {
// 			int32_t i = fr->length;
			
			fstore_push(fs, fr);
// 			int comp = (2*3*5*7);
			/*
			if(comp % i == 0 && sixty == NULL) {
				sixty = fill_fraction(fr, comp);
				free_fraction(fr);
				g_mem_usage += comp;
			}
			else if(sixty && comp % i == 0) {
				add_fraction(sixty, fr);
				free_fraction(fr);
			}
			else {
				fr->data = realloc(fr->data, i);
				VEC_PUSH(&fractions, fr);
				g_mem_usage += i;
			}
			* /
			
		}
		
	}
	
	*/
	
	for(int i = 0; i < num_threads; ++i) {
		pthread_join(threads[i].thread, NULL);
	}
	
	printf("%ld bytes\n", g_mem_usage);
// 	getchar();
	
	
	return 0;
}





static void pretty_print_mem(size_t sz) {
	if(sz >= PETABYTE) {
		printf("%d", (int)(sz / PETABYTE));
// 		printf(".%d", (int)((sz % PETABYTE) / TERABYTE));
		printf("PB");
	}
	else if(sz >= TERABYTE) {
		printf("%d", (int)(sz / TERABYTE));
// 		printf(".%d", (int)((sz % TERABYTE) / GIGABYTE));
		printf("TB");
	}
	else if(sz >= GIGABYTE) {
		printf("%d", (int)(sz / GIGABYTE));
// 		printf(".%d", (int)((sz % GIGABYTE) / MEGABYTE));
		printf("GB");
	}
	else if(sz >= MEGABYTE) {
		printf("%d", (int)(sz / MEGABYTE));
// 		printf(".%d", (int)((sz % MEGABYTE) / KILOBYTE));
		printf("MB");
	}
	else if(sz >= KILOBYTE) {
		printf("%d", (int)(sz / KILOBYTE));
// 		printf(".%d", (int)(sz % KILOBYTE));
		printf("KB");
	}
	else {
		printf("%dB", (int)sz);
	}
}
