// colimabar: Menu bar status of colima: running containers, and what the VMs cost the Mac.
// The menu has a section per running profile, its containers grouped by k3d cluster and compose project.
//
// Usage: colimabar <config>
// Build: make

#import <Cocoa/Cocoa.h>
#include "cb-colima.h"
#include "cb-config.h"

static Config g_config;

static void after(double seconds, dispatch_block_t block) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)), dispatch_get_main_queue(), block);
}

static NSString *bytes(double b) {
    double mib = b / (1 << 20);
    return mib >= 1024 ? [NSString stringWithFormat:@"%.1fG", mib / 1024] : [NSString stringWithFormat:@"%.0fM", mib];
}

// ---------------------------------------------------------------------------
// Menu bar title: SF Symbols inline with text, so both follow the text color
// ---------------------------------------------------------------------------
static NSAttributedString *symbol(NSString *name, double size, NSColor *color) {
    NSTextAttachment *a = [NSTextAttachment new];
    a.image = [[NSImage imageWithSystemSymbolName:name accessibilityDescription:nil]
        imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:size weight:NSFontWeightRegular]];
    NSMutableAttributedString *s = [[NSAttributedString attributedStringWithAttachment:a] mutableCopy];
    [s addAttribute:NSForegroundColorAttributeName value:color range:NSMakeRange(0, s.length)];
    return s;
}

static NSAttributedString *text(NSString *string, NSColor *color) {
    NSFont *font = [NSFont monospacedDigitSystemFontOfSize:[NSFont menuBarFontOfSize:0].pointSize weight:NSFontWeightRegular];
    return [[NSAttributedString alloc] initWithString:string attributes:@{NSFontAttributeName: font, NSForegroundColorAttributeName: color}];
}

static NSAttributedString *title(NSArray<Profile *> *profiles, NSString *phase) {
    double size = [NSFont menuBarFontOfSize:0].pointSize;
    NSColor *label = NSColor.labelColor, *orange = NSColor.systemOrangeColor;
    NSMutableAttributedString *t = [NSMutableAttributedString new];
    if (phase) {
        [t appendAttributedString:symbol(@"arrow.triangle.2.circlepath", size, label)];
        [t appendAttributedString:text([@" " stringByAppendingString:phase], label)];
    } else if (!profiles.count) {
        [t appendAttributedString:symbol(@"shippingbox", size, NSColor.tertiaryLabelColor)];
    } else {
        NSUInteger containers = [[profiles valueForKeyPath:@"@sum.containers"] unsignedIntegerValue];
        double cpu = [[profiles valueForKeyPath:@"@sum.cpu"] doubleValue];
        [t appendAttributedString:symbol(@"shippingbox", size, label)];
        [t appendAttributedString:text([NSString stringWithFormat:@" %lu", containers], containers ? label : orange)];
        if (cpu > g_config.busy_cpu) [t appendAttributedString:text([NSString stringWithFormat:@" %.0f%%", cpu], orange)];
    }
    return t;
}

// ---------------------------------------------------------------------------
// Menu: a section per running profile
// ---------------------------------------------------------------------------

// Attributed titles keep their colors when disabled, so group rows read as content, not as unavailable actions
static NSMenuItem *group_item(Group *g, NSFont *font, NSParagraphStyle *columns) {
    NSDictionary *name = @{NSFontAttributeName: font, NSParagraphStyleAttributeName: columns, NSForegroundColorAttributeName: NSColor.labelColor};
    NSMutableDictionary *numbers = [name mutableCopy];
    numbers[NSForegroundColorAttributeName] = NSColor.secondaryLabelColor;
    NSMutableAttributedString *s = [[NSMutableAttributedString alloc] initWithString:g.name attributes:name];
    [s appendAttributedString:[[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"\t%.0f%%\t%@", g.cpu, bytes(g.mem)] attributes:numbers]];
    NSMenuItem *item = [NSMenuItem new];
    item.attributedTitle = s;
    return item;
}

// CPU and memory right-aligned in columns past the widest group name
static NSParagraphStyle *columns(NSArray<Profile *> *profiles, NSFont *font) {
    double widest = 0;
    for (NSString *name in [profiles valueForKeyPath:@"@unionOfArrays.groups.name"])
        widest = fmax(widest, [name sizeWithAttributes:@{NSFontAttributeName: font}].width);
    NSMutableParagraphStyle *p = [NSMutableParagraphStyle new];
    p.tabStops = @[
        [[NSTextTab alloc] initWithTextAlignment:NSTextAlignmentRight location:widest + 60 options:@{}],
        [[NSTextTab alloc] initWithTextAlignment:NSTextAlignmentRight location:widest + 110 options:@{}],
    ];
    return p;
}

static void add_sections(NSMenu *menu, NSArray<Profile *> *profiles) {
    if (!profiles.count) {
        [menu addItemWithTitle:@"No profile running" action:NULL keyEquivalent:@""];
        [menu addItem:NSMenuItem.separatorItem];
    }
    NSFont *font = [NSFont monospacedDigitSystemFontOfSize:[NSFont menuFontOfSize:0].pointSize weight:NSFontWeightRegular];
    NSParagraphStyle *style = columns(profiles, font);
    for (Profile *p in profiles) {
        [menu addItemWithTitle:[NSString stringWithFormat:@"%@ \u00b7 %@ CPU \u00b7 %@ GiB", p.name, p.cpus, p.memory] action:NULL keyEquivalent:@""];
        [menu addItemWithTitle:[NSString stringWithFormat:@"On Mac: CPU %.0f%% \u00b7 RAM %@", p.cpu, bytes(p.footprint)] action:NULL keyEquivalent:@""];
        if (p.groups.count) [menu addItem:NSMenuItem.separatorItem];
        for (Group *g in p.groups) [menu addItem:group_item(g, font, style)];
        [menu addItem:NSMenuItem.separatorItem];
    }
}

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------
@interface AppDelegate : NSObject <NSApplicationDelegate>
@property NSStatusItem *item;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.item = [NSStatusBar.systemStatusBar statusItemWithLength:NSVariableStatusItemLength];
    self.item.button.title = @"…";
    self.item.menu = [NSMenu new];
    [self poll];
}

- (void)poll {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        NSString *phase = colima_phase();
        NSArray<Profile *> *profiles = running_profiles();
        dispatch_async(dispatch_get_main_queue(), ^{
            [self render:profiles phase:phase];
            after(g_config.status_interval, ^{ [self poll]; });
        });
    });
}

- (void)render:(NSArray<Profile *> *)profiles phase:(NSString *)phase {
    self.item.button.attributedTitle = title(profiles, phase);
    NSMenu *menu = self.item.menu;
    [menu removeAllItems];
    add_sections(menu, profiles);
    [menu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc != 2) { fprintf(stderr, "Usage: colimabar <config>\n"); return 1; }
        g_config = load_config(@(argv[1]));
        AppDelegate *delegate = [AppDelegate new];
        NSApplication.sharedApplication.delegate = delegate;
        [NSApp run];
    }
    return 0;
}
