#ifndef _COMPAT_MACH_VM_MAP_H
#define _COMPAT_MACH_VM_MAP_H
typedef unsigned long vm_address_t;
typedef unsigned long vm_size_t;
typedef int kern_return_t;
typedef unsigned int mach_port_t;
typedef mach_port_t vm_map_t;
kern_return_t vm_allocate(vm_map_t, vm_address_t *, vm_size_t, int);
kern_return_t vm_deallocate(vm_map_t, vm_address_t, vm_size_t);
#endif
