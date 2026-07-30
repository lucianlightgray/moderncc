/* The Objective-C runtime through its pure-C API: class lookup in the dyld
   shared cache, selector interning, objc_msgSend's variadic-cast ABI, and
   runtime class construction. No ObjC syntax -- mcc compiles C. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <objc/runtime.h>
#include <objc/message.h>

static int fails;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		fails++; \
	} \
} while (0)

int main(void) {
	Class nsobject = objc_getClass("NSObject");
	CHECK(nsobject != NULL);
	if (!nsobject) {
		fprintf(stderr, "libsystem_objc: no NSObject; libobjc not loaded\n");
		return 1;
	}
	CHECK(!strcmp(class_getName(nsobject), "NSObject"));
	CHECK(class_getSuperclass(nsobject) == NULL);
	CHECK(objc_getMetaClass("NSObject") != NULL);

	SEL sel_alloc = sel_registerName("alloc");
	SEL sel_init = sel_registerName("init");
	SEL sel_dealloc = sel_registerName("dealloc");
	SEL sel_hash = sel_registerName("hash");
	CHECK(sel_alloc && sel_init && sel_dealloc && sel_hash);
	CHECK(!strcmp(sel_getName(sel_alloc), "alloc"));
	CHECK(sel_registerName("alloc") == sel_alloc);

	CHECK(class_respondsToSelector(object_getClass((id)nsobject), sel_alloc));
	CHECK(class_respondsToSelector(nsobject, sel_init));

	Method m = class_getInstanceMethod(nsobject, sel_hash);
	CHECK(m != NULL);
	if (m) {
		CHECK(method_getName(m) == sel_hash);
		CHECK(method_getImplementation(m) != NULL);
	}

	/* objc_msgSend has no prototype that matches every selector, so every
	   caller casts it -- the ABI detail an incorrect Mach-O stub would break. */
	id (*msg_id)(id, SEL) = (id (*)(id, SEL))objc_msgSend;
	unsigned long (*msg_ul)(id, SEL) = (unsigned long (*)(id, SEL))objc_msgSend;
	void (*msg_void)(id, SEL) = (void (*)(id, SEL))objc_msgSend;

	id obj = msg_id(msg_id((id)nsobject, sel_alloc), sel_init);
	CHECK(obj != NULL);
	if (obj) {
		CHECK(object_getClass(obj) == nsobject);
		CHECK(msg_ul(obj, sel_hash) == (unsigned long)obj);
		msg_void(obj, sel_dealloc);
	}

	/* A runtime-built class exercises the writable half of the runtime. */
	Class made = objc_allocateClassPair(nsobject, "MccDarwinTestClass", 0);
	CHECK(made != NULL);
	if (made) {
		objc_registerClassPair(made);
		CHECK(objc_getClass("MccDarwinTestClass") == made);
		CHECK(class_getSuperclass(made) == nsobject);
		id inst = msg_id(msg_id((id)made, sel_alloc), sel_init);
		CHECK(inst != NULL);
		if (inst) {
			CHECK(!strcmp(object_getClassName(inst), "MccDarwinTestClass"));
			msg_void(inst, sel_dealloc);
		}
	}

	unsigned n = 0;
	Class *all = objc_copyClassList(&n);
	CHECK(n > 0 && all != NULL);
	free(all);

	if (fails) {
		fprintf(stderr, "libsystem_objc: %d failure(s)\n", fails);
		return 1;
	}
	printf("libsystem_objc: OK\n");
	return 0;
}
