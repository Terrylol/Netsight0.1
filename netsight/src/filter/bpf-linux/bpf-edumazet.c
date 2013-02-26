#include <stdio.h>
#include <pcap.h>
#include "filter.h"
#include "bpf-jit-linux.h"
// http://carnivore.it/2011/12/28/bpf_performance#the_ctypes_glue

int compile_to_x86(struct bpf_program *prog, struct sk_filter *filter) {
	int i;
	struct bpf_insn *insns;
	struct sock_filter *sins;
	int ret;

	filter->len = prog->bf_len;
	filter->bpf_func = NULL;
	filter->bpf_size = 0;

	insns = prog->bf_insns;
	sins = filter->insns;

	for (i = 0; i < prog->bf_len; i++) {
		sins->code = insns->code;
		sins->jt = insns->jt;
		sins->jf = insns->jf;
		sins->k = insns->k;
		sins++;
		insns++;
	}

	ret = bpf_jit_compile(filter);
	//printf("bpf_jit_compile returned %d: %p, %d bytes of code.\n",
	//       ret, filter->bpf_func, filter->bpf_size);
    return filter->bpf_size;
}

int compile_to_bpf(char *filter, struct bpf_program *prog) {
	int ret;
	ret = pcap_compile_nopcap(1500, DLT_EN10MB, prog,
				  filter, 1, 0);

	return ret;
}

/*
int main() {	
	struct sk_buff skbuff;
	struct sk_filter filter;
	struct bpf_program program;
	char *hex = "9e6b6e3116e397c781871aee08004500002800000000400657a901010101b84868dd13887530deadbeef000000005002ffff66650000";
	char buff[128];
	int i = 0, ret;
	unsigned long long count = 0;
	int integer = 0;
	char *ptr;

	skbuff.len = 40;
	skbuff.data_len = 40;
	skbuff.data = buff;

	ptr = buff;
	for(i = 0; hex[i]; i++) {
		integer = integer * 16;
		if (hex[i] <= '9')
			integer += hex[i]-'0';
	    else
			integer += hex[i]-'a'+10;
		count++;
		if (count == 2) {
			*ptr++ = integer;
			integer = 0;
			count = 0;
		}
	}
	
	//compile_to_bpf("tcp or udp or icmp", &program);
	compile_to_bpf("(net 130.127.8.0/24 or  net 130.127.9.0/24 or  net 130.127.10.0/24 or  net 130.127.11.0/24 or  net 130.127.28.0/24 or  net 130.127.69.0/24 or  net 130.127.60.0/24 or  net 130.127.235.0/24 or  net 130.127.236.0/24 or  net 130.127.237.0/24 or  net 130.127.238.0/24 or  net 130.127.239.0/24 or  net 130.127.240.0/24 or  net 130.127.241.0/24 or  net 130.127.239.0/24 or  net 130.127.201.0/24 or  net 130.127.255.250/32 or  net 130.127.255.251/32 or  net 130.127.255.246/32 or  net 130.127.255.247/32 or  net 130.127.255.248/32 or  net 172.19.3.0/24)", &program);
	ret = compile_to_x86(&program, &filter);
	printf("Running objdump -b binary -m i386:x86-64:intel  -D code.bin on the code to diasm it.\n");
	system("objdump -b binary -m i386:x86-64:intel  -D code.bin");
	ptr = filter.bpf_func;
	for(i = 0; i < ret; i++) {
		printf("%02x ", ptr[i] & 0xff);
		if (i % 8 == 7) printf("   ");
		if (i % 16 == 15) printf("\n");
	}
    printf("\n");

	int times = 10000000, N = times;
	count = 0;
	struct timeval start, end;
	gettimeofday(&start, NULL);
	while (times--) {
		count += filter.bpf_func(&skbuff, &filter.insns[0]);
	}
	gettimeofday(&end, NULL);
	double dt = end.tv_sec - start.tv_sec;
	dt = dt * 1e6 + (end.tv_usec - start.tv_usec);
	printf("count %llu, %.3lfns/call\n", count, dt * 1e3/N);
	return 0;
}
*/
