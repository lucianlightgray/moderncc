



#include <stdio.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>

static int fails;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		fails++; \
	} \
} while (0)

int main(void) {
	mach_port_t self = mach_task_self();
	CHECK(self != MACH_PORT_NULL);




	mach_msg_type_number_t cnt = TASK_BASIC_INFO_64_COUNT;
	struct task_basic_info_64 tbi;
	memset(&tbi, 0, sizeof tbi);
	CHECK(task_info(self, TASK_BASIC_INFO_64, (task_info_t)&tbi, &cnt) == KERN_SUCCESS);
	CHECK(tbi.resident_size > 0);
	CHECK(tbi.virtual_size > 0);

	mach_port_t host = mach_host_self();
	CHECK(host != MACH_PORT_NULL);
	host_basic_info_data_t hbi;
	mach_msg_type_number_t hcnt = HOST_BASIC_INFO_COUNT;
	memset(&hbi, 0, sizeof hbi);
	CHECK(host_info(host, HOST_BASIC_INFO, (host_info_t)&hbi, &hcnt) == KERN_SUCCESS);
	CHECK(hbi.max_cpus > 0);
	CHECK(hbi.memory_size > 0);
	CHECK(mach_port_deallocate(self, host) == KERN_SUCCESS);

	vm_size_t page = 0;
	CHECK(host_page_size(mach_host_self(), &page) == KERN_SUCCESS);
	CHECK(page >= 4096);


	mach_vm_address_t addr = 0;
	mach_vm_size_t len = page * 4;
	CHECK(mach_vm_allocate(self, &addr, len, VM_FLAGS_ANYWHERE) == KERN_SUCCESS);
	CHECK(addr != 0);
	if (addr) {
		unsigned char *p = (unsigned char *)addr;
		int zeros = 0;
		for (mach_vm_size_t i = 0; i < len; i++)
			zeros += (p[i] == 0);
		CHECK((mach_vm_size_t)zeros == len);
		memset(p, 0x5A, (size_t)len);
		CHECK(p[0] == 0x5A && p[len - 1] == 0x5A);
		CHECK(mach_vm_protect(self, addr, len, 0, VM_PROT_READ) == KERN_SUCCESS);
		CHECK(p[0] == 0x5A);
		CHECK(mach_vm_deallocate(self, addr, len) == KERN_SUCCESS);
	}


	mach_timebase_info_data_t tb;
	memset(&tb, 0, sizeof tb);
	CHECK(mach_timebase_info(&tb) == KERN_SUCCESS);
	CHECK(tb.numer > 0 && tb.denom > 0);
	uint64_t t0 = mach_absolute_time();
	volatile unsigned long spin = 0;
	for (unsigned long i = 0; i < 20000000UL; i++)
		spin += i;
	uint64_t t1 = mach_absolute_time();
	CHECK(t1 > t0);
	uint64_t ns = (t1 - t0) * tb.numer / tb.denom;
	CHECK(ns > 1000);


	mach_port_t port = MACH_PORT_NULL;
	CHECK(mach_port_allocate(self, MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS);
	CHECK(port != MACH_PORT_NULL);
	if (port != MACH_PORT_NULL) {
		CHECK(mach_port_insert_right(self, port, port,
									 MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);
		mach_port_urefs_t refs = 0;
		CHECK(mach_port_get_refs(self, port, MACH_PORT_RIGHT_SEND, &refs) == KERN_SUCCESS);
		CHECK(refs == 1);
		CHECK(mach_port_deallocate(self, port) == KERN_SUCCESS);
		CHECK(mach_port_mod_refs(self, port, MACH_PORT_RIGHT_RECEIVE, -1) == KERN_SUCCESS);
	}

	if (fails) {
		fprintf(stderr, "libsystem_mach: %d failure(s)\n", fails);
		return 1;
	}
	printf("libsystem_mach: OK\n");
	return 0;
}
