// command-not-found, finds programs
// Copyright (C) 2017  Shawn Landden <shawn@git.icu>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2.1
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Inspired by https://github.com/pts/pts-line-bisect
// by Péter Szabó <pts@fazekas.hu>
const mem = @import("std").mem;

fn getFofs(file: []const u8, constofs: usize, seperator: u8) usize {
    var ofs = constofs;
    if (ofs == 0)
        return 0;
    if (ofs > file.len)
        return file.len;
    ofs -= 1;
    var maybe_end = mem.indexOfScalarPos(u8, file, ofs, seperator);
    if (maybe_end) |end|
        return end + 1;
    var maybe_start = mem.lastIndexOfScalar(u8, file[0..ofs], seperator);
    if (maybe_start) |start|
        return start + 1;
    return 0;
}

fn compareLine(file: []const u8, fofs: usize, x: []const u8) mem.Compare {
    var len: usize = undefined;

    if (fofs == file.len)
        return mem.Compare.GreaterThan;
    if (x.len < (file.len - fofs)) {
        len = x.len;
    } else {
        len = file.len - fofs;
    }

    return mem.compare(u8, file[fofs..fofs + len], x[0..len]);
}

fn bisectWay(file: []const u8, x: []const u8, seperator: u8) ?[]const u8 {
    var mid: usize = undefined;
    var midf: usize = undefined;
    var lo: usize = 0;
    var hi: usize = @maxValue(usize);
    var cmp: mem.Compare = undefined;
    if (hi > file.len)
        hi = file.len;
    if (x.len == 0)
        if (hi == file.len)
            return file[hi..file.len];
    if (lo >= hi) {
        var start = getFofs(file, lo, seperator);
        return file[start..file.len];
    }
    while (true) {
        mid = (lo + hi) >> 1;
        midf = getFofs(file, mid, seperator);
        cmp = compareLine(file, midf, x);
        if (cmp == mem.Compare.GreaterThan) {
            hi = mid;
        } else if (cmp == mem.Compare.LessThan) {
            lo = mid + 1;
        } else if (cmp == mem.Compare.Equal) {
            return file[midf..file.len];
        }
        if (lo >= hi)
            break;
    }
    return null;
}

/// UTF-8 has two unused bytes 0b1111111x, which can be used as seperators amd delimiters.
/// RFC 3629, which limits the range of Unicode to U+10FFFF,
/// adds 6 more: 0b111110xx, 0b1111110x.
pub fn search(file: []const u8, search_augmented: []const u8, seperator: u8) ?[]const u8 {
    var maybe_result = bisectWay(file, search_augmented, seperator);
    if (maybe_result) |*result| {
        if (mem.indexOfScalar(u8, result.*, seperator)) |result_end| {
            result.* = result.*[0..result_end];
        }
        return result.*;
    }
    return null;
}

pub fn main() void {
    const os = @import("std").os;
    const linux = @import("std").os.linux;
    const posix = @import("std").os.posix;
    var st: posix.Stat = undefined;

    var db_fd = os.posixOpenC(c"/var/cache/command-not-found/db", posix.O_RDONLY, 0) catch unreachable;
    _ = linux.fstat(db_fd, &st);
    var db = @intToPtr([*]u8, linux.mmap(null, @intCast(usize, st.size), linux.PROT_READ, linux.MAP_SHARED, db_fd, 0))[0..@intCast(usize, st.size)];
    var maybe_result = search(db, "hexchat\xff"[0..], '\n');
    if (maybe_result) |result| {
        const io = @import("std").io;
        var stdout_file = io.getStdOut() catch unreachable;
        stdout_file.write(result) catch unreachable;
    }
}
