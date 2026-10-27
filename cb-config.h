// cb-config.h: key=value config file

#ifndef CB_CONFIG_H
#define CB_CONFIG_H

#import <Foundation/Foundation.h>

typedef struct {
    double status_interval, busy_cpu;
} Config;

static NSDictionary<NSString *, NSString *> *parse_config(NSString *text) {
    NSMutableDictionary *d = [NSMutableDictionary new];
    for (NSString *line in [text componentsSeparatedByString:@"\n"]) {
        NSRange eq = [line rangeOfString:@"="];
        if (eq.location == NSNotFound || [line hasPrefix:@"#"]) continue;
        d[[line substringToIndex:eq.location]] = [line substringFromIndex:NSMaxRange(eq)];
    }
    return d;
}

static Config load_config(NSString *path) {
    NSString *text = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    if (!text) { fprintf(stderr, "Cannot read %s\n", path.UTF8String); exit(1); }
    NSDictionary *d = parse_config(text);
    NSString *(^str)(NSString *) = ^(NSString *key) {
        if (!d[key]) { fprintf(stderr, "Missing %s in %s\n", key.UTF8String, path.UTF8String); exit(1); }
        return (NSString *)d[key];
    };
    return (Config){
        .status_interval = str(@"status_interval").doubleValue,
        .busy_cpu = str(@"busy_cpu").doubleValue,
    };
}

#endif // CB_CONFIG_H
