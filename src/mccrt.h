/* Process lifetime: one place where the process-wide subsystems start, one
 * place where they stop.
 *
 * Two subsystems own a resource that outlives any single compilation -- the GPU
 * device and the JIT worker pool -- and both used to start lazily and stop from
 * an atexit handler registered by whichever lazy path happened to run first.
 * That is not a style complaint, it produced two concrete defects:
 *
 *   - The teardown order between them was accidental. mccjit_embed.c says so in
 *     its own comment: "the pair came out right only because boot_swap_async
 *     calls them in that order; the mccjit_kgc_reg path registers this handler
 *     independently and can invert it." Joining the worker pool has to happen
 *     before the device is destroyed, or a worker can be inside a dispatch when
 *     the device goes away.
 *
 *   - Tearing the device down FROM an atexit handler races the Vulkan driver's
 *     own unload. mccast.c says so in its own comment: registering one more
 *     handler at a different moment "turns mcc_gpu_quiesce into a call through
 *     an unmapped page". Anything that ran before the C runtime started
 *     unwinding would not have that problem.
 *
 * So: call mcc_rt_enter() as the first statement of main and mcc_rt_exit() as
 * the last. Nothing else starts or stops these subsystems. The order inside
 * mcc_rt_exit() is a property of the code rather than of a registration
 * sequence, and it runs before atexit and before the driver unloads.
 *
 * Both calls are idempotent and safe in a process that uses neither subsystem;
 * the GPU is opt-in (MCC_AST_EVAL_LADDER_GPU or an explicit force), so a
 * process that never asks for it pays nothing here.
 */
#ifndef MCC_RT_H
#define MCC_RT_H

/* First statement of main. Establishes the process-wide subsystems. */
void mcc_rt_enter(void);

/* Last statement of main, on every return path. Stops them, in order. */
void mcc_rt_exit(void);

/* Installed by the compiler side, which owns the GPU opt-in; null in tools that
 * link mccgpu.c directly and manage their own device. */
void mcc_rt_set_gpu_boot(void (*fn)(void));

/* Nonzero between the two. Lets a lazy path assert that it is not bringing a
 * process-wide resource up after main has begun tearing down. */
int mcc_rt_running(void);

#endif
