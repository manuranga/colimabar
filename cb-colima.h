// cb-colima.h: Running colima profiles, their containers and what their VMs cost the Mac
// Blocking; call off the main thread.

#ifndef CB_COLIMA_H
#define CB_COLIMA_H

#include "cb-docker.h"
#include "cb-proc.h"

@interface Profile : NSObject
@property NSString *name, *cpus, *memory;  // as in colima.yaml; memory in GiB
@property double cpu, footprint;           // the VM on the Mac: percent of one core, bytes
@property NSUInteger containers;
@property NSArray<Group *> *groups;
@end

@implementation Profile
@end

static NSString *colima_dir(void) {
    return [NSHomeDirectory() stringByAppendingPathComponent:@".colima"];
}

// Top-level scalar, enough for colima.yaml's cpu and memory
static NSString *yaml_value(NSString *yaml, NSString *key) {
    NSString *prefix = [key stringByAppendingString:@": "];
    for (NSString *line in [yaml componentsSeparatedByString:@"\n"])
        if ([line hasPrefix:prefix]) return [line substringFromIndex:prefix.length];
    return nil;
}

static NSString *instance_dir(NSString *profile) {
    NSString *instance = [profile isEqual:@"default"] ? @"colima" : [@"colima-" stringByAppendingString:profile];
    return [NSString pathWithComponents:@[colima_dir(), @"_lima", instance]];
}

// Whether lima's hostagent, which runs the VM, is alive
static bool is_running(NSString *profile) {
    NSString *file = [instance_dir(profile) stringByAppendingPathComponent:@"ha.pid"];
    pid_t pid = [NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:nil].intValue;
    return pid > 0 && kill(pid, 0) == 0;
}

// Takes at least a second, to measure the VM's CPU
static Profile *probe_profile(NSString *name) {
    NSString *dir = [colima_dir() stringByAppendingPathComponent:name];
    NSString *yaml = [NSString stringWithContentsOfFile:[dir stringByAppendingPathComponent:@"colima.yaml"] encoding:NSUTF8StringEncoding error:nil];
    NSString *socket_path = [dir stringByAppendingPathComponent:@"docker.sock"];
    NSDate *start = NSDate.date;
    Usage before = vm_usage(instance_dir(name));
    NSArray<NSDictionary *> *containers = docker_get(socket_path, @"/containers/json");
    NSData *stats = containers_stats(socket_path, containers);
    [NSThread sleepUntilDate:[start dateByAddingTimeInterval:1]];
    Usage after = vm_usage(instance_dir(name));

    Profile *p = [Profile new];
    p.name = name;
    p.cpus = yaml_value(yaml, @"cpu");
    p.memory = yaml_value(yaml, @"memory");
    p.cpu = cpu_percent(before, after);
    p.footprint = after.footprint;
    p.containers = containers.count;
    p.groups = grouped(containers, stats.bytes);
    return p;
}

// Profiles are the entries of ~/.colima with a lima hostagent
static NSArray<Profile *> *running_profiles(void) {
    NSMutableArray *profiles = [NSMutableArray new];
    NSArray *names = [[NSFileManager.defaultManager contentsOfDirectoryAtPath:colima_dir() error:nil] sortedArrayUsingSelector:@selector(compare:)];
    for (NSString *name in names)
        if (is_running(name)) [profiles addObject:probe_profile(name)];
    return profiles;
}

#endif // CB_COLIMA_H
