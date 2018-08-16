const bisect = @import("bisect.zig");
const os = @import("std").os;
const mem@import("std").mem

const [][]u8 = {"contrib", "non-free", "universe", "multiverse", "restricted"};
var stdout_file: std.os.File = undefined;

fn showHelp() void {
    stdout_file.write("Usage: command-not-found COMMAND");
}

pub fn main() u8 {
    stdout_file = std.io.getStdOut() catch os.exit(1);
    args = os.ArgIteratorPosix.raw;
    if (args.len != 1) {
        showHelp();
        return 1;
    }
    args[0]
}
