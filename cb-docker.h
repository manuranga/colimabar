// cb-docker.h: Blocking Docker Engine API client over a unix socket, and container grouping
// Call off the main thread.

#ifndef CB_DOCKER_H
#define CB_DOCKER_H

#import <Foundation/Foundation.h>
#include <sys/socket.h>
#include <sys/un.h>

@interface Group : NSObject
@property NSString *name;
@property double cpu, mem;  // percent of one core, bytes
@end

@implementation Group
@end

typedef struct {
    double cpu, mem;
} Stats;

// JSON body of a 200 reply, else nil
static id parse_reply(NSData *reply) {
    NSRange head = [reply rangeOfData:[@"\r\n\r\n" dataUsingEncoding:NSASCIIStringEncoding] options:0 range:NSMakeRange(0, reply.length)];
    if (head.location == NSNotFound || head.location < 12) return nil;
    NSString *status = [[NSString alloc] initWithData:[reply subdataWithRange:NSMakeRange(9, 3)] encoding:NSASCIIStringEncoding];
    if (![status isEqual:@"200"]) return nil;
    NSData *body = [reply subdataWithRange:NSMakeRange(NSMaxRange(head), reply.length - NSMaxRange(head))];
    return [NSJSONSerialization JSONObjectWithData:body options:0 error:nil];
}

// HTTP/1.0, so the reply is unchunked and ends when the engine closes the connection
static id docker_get(NSString *socket_path, NSString *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strlcpy(addr.sun_path, socket_path.fileSystemRepresentation, sizeof addr.sun_path);
    int on = 1;
    struct timeval timeout = {5, 0};
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    NSMutableData *reply = [NSMutableData new];
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0) {
        NSData *req = [[NSString stringWithFormat:@"GET %@ HTTP/1.0\r\n\r\n", path] dataUsingEncoding:NSASCIIStringEncoding];
        write(fd, req.bytes, req.length);
        char buf[65536];
        ssize_t n;
        while ((n = read(fd, buf, sizeof buf)) > 0) [reply appendBytes:buf length:n];
    }
    close(fd);
    return parse_reply(reply);
}

// As `docker stats`: CPU over the engine's own one-second sample, memory without inactive page cache
static Stats parse_stats(NSDictionary *d) {
    NSDictionary *cpu = d[@"cpu_stats"], *pre = d[@"precpu_stats"], *mem = d[@"memory_stats"];
    double used = [cpu[@"cpu_usage"][@"total_usage"] doubleValue] - [pre[@"cpu_usage"][@"total_usage"] doubleValue];
    double system = [cpu[@"system_cpu_usage"] doubleValue] - [pre[@"system_cpu_usage"] doubleValue];
    return (Stats){
        .cpu = system > 0 ? used / system * [cpu[@"online_cpus"] doubleValue] * 100 : 0,
        .mem = [mem[@"usage"] doubleValue] - [mem[@"stats"][@"inactive_file"] doubleValue],
    };
}

// In parallel, since each waits for the engine's one-second sample. Returns Stats[containers.count].
static NSData *containers_stats(NSString *socket_path, NSArray<NSDictionary *> *containers) {
    NSMutableData *data = [NSMutableData dataWithLength:containers.count * sizeof(Stats)];
    Stats *stats = data.mutableBytes;
    dispatch_apply(containers.count, DISPATCH_APPLY_AUTO, ^(size_t i) {
        stats[i] = parse_stats(docker_get(socket_path, [NSString stringWithFormat:@"/containers/%@/stats?stream=false", containers[i][@"Id"]]));
    });
    return data;
}

static NSString *group_name(NSDictionary *container) {
    NSDictionary *labels = container[@"Labels"];
    if (labels[@"k3d.cluster"]) return [@"k3d " stringByAppendingString:labels[@"k3d.cluster"]];
    if (labels[@"com.docker.compose.project"]) return [@"compose " stringByAppendingString:labels[@"com.docker.compose.project"]];
    return [container[@"Names"][0] substringFromIndex:1];
}

// Busiest first
static NSArray<Group *> *grouped(NSArray<NSDictionary *> *containers, const Stats *stats) {
    NSMutableDictionary<NSString *, Group *> *groups = [NSMutableDictionary new];
    for (NSUInteger i = 0; i < containers.count; i++) {
        NSString *name = group_name(containers[i]);
        Group *g = groups[name];
        if (!g) {
            g = groups[name] = [Group new];
            g.name = name;
        }
        g.cpu += stats[i].cpu;
        g.mem += stats[i].mem;
    }
    return [groups.allValues sortedArrayUsingDescriptors:@[
        [NSSortDescriptor sortDescriptorWithKey:@"cpu" ascending:NO],
        [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES],
    ]];
}

#endif // CB_DOCKER_H
