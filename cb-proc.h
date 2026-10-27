// cb-proc.h: Process queries via libproc and sysctl

#ifndef CB_PROC_H
#define CB_PROC_H

#import <Foundation/Foundation.h>
#include <libproc.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>

typedef struct {
    double time, cpu, footprint;  // ns, CPU ns, bytes
} Usage;

static void each_pid(void (^f)(pid_t)) {
    pid_t pids[8192];
    int n = proc_listallpids(pids, sizeof pids);
    for (int i = 0; i < n; i++) f(pids[i]);
}

// rusage CPU times are in mach ticks
static double cpu_ns(const struct rusage_info_v4 *r) {
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    return (double)(r->ri_user_time + r->ri_system_time) * tb.numer / tb.denom;
}

static bool has_open_file_in(pid_t pid, const char *dir) {
    struct proc_fdinfo fds[1024];
    int n = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fds, sizeof fds) / PROC_PIDLISTFD_SIZE;
    for (int i = 0; i < n; i++) {
        struct vnode_fdinfowithpath info;
        if (fds[i].proc_fdtype == PROX_FDTYPE_VNODE
            && proc_pidfdinfo(pid, fds[i].proc_fd, PROC_PIDFDVNODEPATHINFO, &info, sizeof info) == sizeof info
            && !strncmp(info.pvip.vip_path, dir, strlen(dir))) return true;
    }
    return false;
}

// The Virtualization.framework service running the VM of a lima instance, found by its open disk and logs.
// Its parent is launchd, and macOS holds whoever ran `colima start` responsible for it, e.g. the terminal.
static Usage vm_usage(NSString *instance_dir) {
    const char *dir = [instance_dir stringByAppendingString:@"/"].fileSystemRepresentation;
    __block Usage u = {clock_gettime_nsec_np(CLOCK_UPTIME_RAW), 0, 0};
    each_pid(^(pid_t pid) {
        char name[2 * MAXCOMLEN + 1];
        struct rusage_info_v4 r;
        if (proc_name(pid, name, sizeof name) <= 0 || strncmp(name, "com.apple.Virtualization.", 25) || !has_open_file_in(pid, dir)
            || proc_pid_rusage(pid, RUSAGE_INFO_V4, (rusage_info_t *)&r)) return;
        u.cpu += cpu_ns(&r);
        u.footprint += r.ri_phys_footprint;
    });
    return u;
}

// Percent of one core between two usages of the same processes
static double cpu_percent(Usage a, Usage b) {
    return b.time > a.time ? fmax(0, b.cpu - a.cpu) / (b.time - a.time) * 100 : 0;
}

// KERN_PROCARGS2 layout: argc, exec path, NUL padding, argv, env
static NSArray<NSString *> *parse_procargs(const char *buf, size_t size) {
    int argc = *(const int *)buf;
    const char *p = buf + sizeof argc, *end = buf + size;
    p += strnlen(p, end - p);
    NSMutableArray *args = [NSMutableArray new];
    while ((int)args.count < argc) {
        while (p < end && !*p) p++;
        if (p == end) break;
        size_t n = strnlen(p, end - p);
        [args addObject:[[NSString alloc] initWithBytes:p length:n encoding:NSUTF8StringEncoding] ?: @""];
        p += n;
    }
    return args;
}

static NSArray<NSString *> *process_args(pid_t pid) {
    size_t size = ARG_MAX;
    NSMutableData *buf = [NSMutableData dataWithLength:size];
    int mib[] = {CTL_KERN, KERN_PROCARGS2, pid};
    if (sysctl(mib, 3, buf.mutableBytes, &size, NULL, 0)) return @[];
    return parse_procargs(buf.bytes, size);
}

// "starting" or "stopping" while `colima start` or `colima stop` runs, for any profile
static NSString *colima_phase(void) {
    __block NSString *phase;
    each_pid(^(pid_t pid) {
        char name[2 * MAXCOMLEN + 1];
        if (proc_name(pid, name, sizeof name) <= 0 || strcmp(name, "colima")) return;
        NSArray *args = process_args(pid);
        if (args.count > 1) phase = @{@"start": @"starting", @"stop": @"stopping"}[args[1]] ?: phase;
    });
    return phase;
}

#endif // CB_PROC_H
